# 系统架构

## 1. 总体设计

两台设备使用相同的 ESP32-C3 Super Mini 和 Ra-01SCH
硬件，因此采用一份代码和一个固件镜像。

``` text
                    同一份固件镜像
                           |
                          Boot
                           |
                    从 NVS 读取配置
                           |
                      验证配置
                           |
            +--------------+--------------+
            |              |              |
         GATEWAY         CLIENT        配置无效
            |              |              |
        GatewayApp      ClientApp     Provisioning
                                      (禁止正常 TX)
```

角色由 NVS
配置决定，不通过硬件自动识别。两台设备硬件相同，固件无法仅凭硬件判断哪台设备应当作为
Gateway。Gateway、Client
和共享协议应放在同一个代码仓库中，尽量使用同一个 Codex 工作区；按
milestone 分配任务，不要拆成两个独立且容易产生差异的代码库。

## 2. 启动流程

1.  初始化串口日志。
2.  初始化 NVS 配置存储。
3.  读取配置版本和设备参数。
4.  验证角色、Network ID、Node ID、Gateway ID。
5.  配置无效时进入配置模式，禁止正常无线发送。
6.  配置有效时只创建对应的 GatewayApp 或 ClientApp。
7.  通过唯一的 RadioDriver 初始化 LLCC68。
8.  在主循环中调用应用的非阻塞 update 方法。

配置版本不兼容时不能默认选择 Gateway 或 Client。

## 3. 串口配置命令

串口速率：115200 baud。

``` text
help
status
config show
config set role gateway|client
config set network-id <1..65535>
config set node-id <1..65534>
config set gateway-id <1..65534>
config save
reboot
```

初始测试配置：

| 参数       | Gateway | Client |
|------------|--------:|-------:|
| Role       | GATEWAY | CLIENT |
| Network ID |       1 |      1 |
| Node ID    |       1 |      2 |
| Gateway ID |       1 |      1 |

节点/Gateway ID 0 和 65535 为保留值。Gateway 的 `node_id` 必须等于
`gateway_id`；Client 的 `node_id` 必须与 `gateway_id`
不同。角色变更后重启生效。

实现补充（ADR-009）：128 字节行缓冲区、最多 6 个 token、每次 loop 最多处理 64 字节。超长行或控制字节使整行丢弃至换行。新增 `config set hello-interval-ms <1000..3600000>`；默认事务结束后等待 5000 ms。角色命令保留 ID；Network ID 65535 合法，65535 广播保留值只适用于节点地址。

`config show` 显示暂存值，`status` 显示启动加载值；加载失败时用 `inactive_defaults` 标识默认建议值。schema 1 使用带完整性校验的 24 字节单 blob，保存后读回验证；校验不是身份认证。无效加载保留 UNSET，不初始化无线模块。重启请求停止新 TX，等待当前 TX 完成或超时，进入 standby 后重启。初始化失败须检查后重启；RX 启动最多连续恢复 3 次，间隔 1 秒。

NVS namespace/key 为 `legacy schema-1 namespace` / `device`，所有多字节字段小端：offset 0/u32 magic=0x4344464D，4/u16 schema=1，6/u8 role（1 Gateway/2 Client），7/u8 reserved=0，8/u16 network，10/u16 node，12/u16 gateway，14/u16 reserved=0，16/u32 HELLO interval(ms)，20/u32 FNV-1a。FNV 覆盖字节 0–19，初值 2166136261，每字节 xor 后乘 16777619、按 2^32 取模；这不是无线协议扩展。

NvsConfigStore 使用数值常量保留原 namespace 字节，确保旧配置仍可读取。该值只用于存储兼容，不是项目身份或应用依赖；公开文档不保留旧标签。schema/key/blob 和空中协议字节不变。

## 4. 模块职责

| 模块              | 职责                               |
|-------------------|------------------------------------|
| `main.cpp`        | Arduino 入口，只调用 Runtime       |
| `FirmwareRuntime` | 加载配置并分配运行角色             |
| `DeviceConfig`    | 独立于 Arduino 的配置模型和验证    |
| `NvsConfigStore`  | 通过 ConfigStorage 验证/编解码、错误分类和读回比较 |
| `PreferencesConfigStorage` | 唯一 Preferences owner，五个直接操作 |
| `GatewayApp`      | Gateway 应用状态机                 |
| `ClientApp`       | Client 应用状态机                  |
| `RadioDriver`     | 通过 RadioHardware 引用拥有状态/错误策略 |
| `Llcc68Hardware`  | 唯一 Module/LLCC68/SPI/GPIO/ISR adapter |
| `FrameCodec`      | 独立于 Arduino/RadioLib 的帧编解码 |
| `SequenceTracker` | 重复包识别                         |
| `SerialLogger`    | 结构化日志                         |

禁止将协议解析、RadioLib 调用、重传逻辑或 ADR 决策放入 `main.cpp`。

## 5. 后续 MAC 设计

后续采用 Gateway-Scheduled TDMA、保守 ADR、ACK Bitmap 和选择性重传。

Gateway 只有一个 LLCC68，不能同时接收多个 SF。必须按照调度表逐个切换接收
PHY。

当前库存为两个无线模块、一个 MCU，第二个 MCU 尚未购买，天线尚未制作；还没有组装双设备。未来两台设备的测试不能证明多节点或 70
节点性能。

## 6. 软件验证边界（ADR-010）

GatewayApp/ClientApp 只依赖 6 方法 RadioTransport 引用，RadioDriver 实现接口并通过唯一 Llcc68Hardware adapter 操作 LLCC68/SPI/IRQ；Runtime 继续调用具体 driver 的 begin/poll/stop。Native 使用有界 fake transport、虚拟时钟和 Serial 适配，编译实际生产 app/codec/tracker/config/logger，不复制状态机逻辑，也不为接口引入运行时堆分配。

应用采用协作式推进，但 NVS commit、RadioLib BUSY 等待和 SDK 串口调用仍是同步操作；有界日志队列不代表 SDK 硬实时非阻塞。RadioLib 默认也会分配 SPI 临时缓冲区。Host 测试不验证硬件延迟、ISR 竞态、Flash 或 RF。

M2 不新增自动重传；测试只重放完全相同的捕获字节验证身份/去重。超时后的新事务仍使用新 sequence。

## 7. 独立集成架构：当前实现与目标

| 边界 | 当前可从源码确认的实现 | 尚未实现的目标 |
|---|---|---|
| 协议核 | 显式 LE FrameCodec、packet identity、sequence/session、M2 HELLO/ACK 状态机 | 无 TDMA/superframe/bitmap/selective retry/ADR/通用 DATA 服务；app 仍直接调用 ESP timer 和具体 SerialLogger，因此不是完整跨平台核心 |
| Radio | 6 方法 RadioTransport、有界事件、RadioDriver 状态 owner 和唯一 Llcc68Hardware adapter | init/poll/stop 仍为具体 driver 入口；RadioHardware 只隔离现有固定 profile 硬件调用供测试，无通用配置服务或 SX1262 adapter |
| Platform | ESP32/Arduino runtime、NVS、console、pins、IRQ、random/timer | 已有窄 ConfigStorage session/bytes seam 供 store 故障测试；无通用 clock/random/interrupt 或其他 MCU 生产端口 |
| Application | 当前 Client 发空 HELLO，codec 可处理有界不解释字节，serial 记录链路事件/计数 | 无 payload 提交、接收 callback、event subscriber 或通用统计接口，需另行审查实现 |

Modbus、电表模型、Wi-Fi/MQTT、数据库和 dashboard 不在协议核中，源码也未实现。未来集成应通过 application 边界；工业能耗监测是通用研究动机，不是必需部署。Iteration 008 仅改身份符号及文档，Iteration 009 只增加 driver 验证 seam；不补齐通用集成 API、不实现后续 MAC。依赖同步等待/分配及实物验证限制保持。

## 8. Driver 故障验证边界（ADR-012）

实际 RadioDriver 不依赖 Arduino/RadioLib，begin/receive/transmit/stop/poll 通过非拥有 RadioHardware 引用在 native 执行。Llcc68Hardware 唯一拥有 Module/LLCC68/SPI/GPIO/IRQ，固定参数原样移动；Runtime 先构造 adapter 再 driver，应用 6 方法 RadioTransport 不变。ISR 仍只置 volatile flag，采样后清除仍依赖锁存高电平 DIO1，不能证明硬件竞态安全。

有界 fake 注入返回码、IRQ 锁存/电平/flags、包字节和显式 uint32 时间，执行生产策略而非模拟状态机。Host 不执行真实 adapter、芯片命令、RadioLib 内部或延迟；生产 static_assert 核对 status/raw IRQ 常量，固定调用参数另行审查。平台/application API、同步等待/分配与实物门槛仍保留。

## 9. 配置存储验证边界（ADR-013）

NvsConfigStore 通过非拥有 ConfigStorage 引用执行五操作，保留实际验证/编解码、错误分类/顺序及读回比较。PreferencesConfigStorage 唯一拥有 Preferences，无配置策略；Runtime 先 adapter 后 store，无堆所有权或通用存储框架。数值 namespace 仍在 store 且原样保留；device key/schema/24-byte blob、DeviceConfig/console/runtime 实现不变。

load 只读打开、查长度、最多读 24 bytes、关闭后 decode；save 先验证/encode，再可写打开/写/关闭，然后实际 load 新开只读会话并精确比较编码字节。每次成功 begin 都显式 end，失败 begin 不 end，维持现有串行非重入约定，不增加并发/嵌套事务。

Native 执行实际 store/config/codec，只模拟低层响应，验证错误/清理/readback 策略，不执行真实 adapter/NVS 内部或 Flash。失败不承诺回滚，host readback 成功不证明实物 commit/断电原子性/持久性。Runtime 无效配置先于 radio.begin 返回、保存后暂存直到重启的策略仅保留/源码核对，本套件未执行 runtime dispatcher。
