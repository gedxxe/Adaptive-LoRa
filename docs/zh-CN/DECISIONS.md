# 架构决策补充

英文 `docs/en/DECISIONS.md` 为完整规范记录。ADR-001–008 已确定：一份 NVS 选角色固件、LLCC68/RadioLib 7.8.1、先固定 PHY HELLO/ACK、后续 Gateway TDMA/bitmap/保守 ADR、M0–M2 不做云端/电表集成，以及薄 main.cpp 架构。

## ADR-009 — M2 有界存储、解析、去重和诊断

- 接受日期：2026-11-12；实现候选，实物验证待完成。
- NVS schema 1：显式 24 字节小端单 blob（magic/schema/role/IDs/interval/reserved/FNV-1a）。完整性校验检测存储损坏，不是无线校验或认证；保存验证完整配置并读回，失败加载保持 UNSET；重启后才激活暂存配置。
- Console：128 字节行、最多 6 token、每 loop 64 字节。新增 `config set hello-interval-ms <1000..3600000>`，默认事务结束后等待 5000 ms。角色编辑保留 ID；Network 65535 合法，广播保留值适用于节点地址。
- HELLO Payload 为 0–68 不解释字节，当前 Client 发空；ACK 恰好 9 字节、仅 status=0。M2 拒绝广播、未来类型及非零 frame/config/schedule 字段，v1 Header 不变。
- 去重：8 个 session，每个 64 sequence 窗口；更旧序号视为 stale。旧 session 淘汰或 Gateway 重启会丢历史，不提供持久化/认证重放保护，不实现 DATA。
- 固定 SF7、sync 0x12、XTAL、LDO，保留默认内部 DIO2 RF-switch 控制；TXEN/RXEN 不接 MCU，须核实实际模块。原生 USB CDC 和全部引脚仍为暂定。
- 日志：16 行有界队列，非阻塞排空；status 报告丢弃计数。丢日志时重做验收。库未公开匹配芯片标识字符串，不能伪造检测结果；未知指标使用 N/A。

## ADR-010 — 用最小 transport 引用验证生产 M2 状态机

- 接受日期 2026-11-12。先只读审查和提出方案，再实现 6 个现有 radio 方法的 RadioTransport；RadioDriver 保持唯一 LLCC68/SPI/IRQ owner，Runtime 保持具体 driver 的 begin/poll/stop。无 factory、堆所有权或额外调度框架。
- Native 编译实际 app/codec/tracker/config/logger；只替换 transport 完成事件、ESP 计时及 Serial 字节适配。它不是 LLCC68/RF/ISR/NVS 仿真。Runtime/NVS 本次为静态审查，不声称被 app integration tests 执行。
- 不添加 M2 自动重传。测试重放完全相同字节；超时后的新事务使用新序号。去重缓存和 Gateway 重启丢历史的现有边界保持。
- 更正 ADR-009 非阻塞表述：有界队列不证明 SDK USB 调用非阻塞；NVS/BUSY/USB 延迟、RadioLib 临时堆分配仍是未验证的边界。
- 实际库存两个模块/一个 MCU/无制作天线，未组装双设备。全部实物门槛保留，后续 MAC 不实施。

## ADR-011 — Adaptive-LoRa 独立身份和发布边界

- 2026-11-12 接受；固件 0.2.0-alpha.1、协议 1 不变。
- 项目为 Adaptive-LoRa；研究标题为 Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks。工业能耗是通用动机，不是协议依赖或部署关系。
- 按明确授权脱敏历史公开副本的身份/路径，保留顺序、时间戳、版本、结果、错误及解决状态。原始私有副本在发布树外，旧 hash 仍标识原工件；公开 digest/脱敏清单，不把改名解释为重新验证。
- 仅机械改 C++ namespace/native 宏；旧 NVS namespace 字节用数值兼容常量保留，schema/key/blob、空中字段、pins/PHY/状态逻辑不变，不做存储迁移或协议升级。
- 保持现有 RadioTransport/LLCC68 owner，明确 platform/application API 缺口，不声称已有通用 payload/event、跨 MCU 服务或 SX1262 实现；电表/回传/数据库/UI 必须在协议外。
- 只在项目根初始化 main，保留远端已有提交，审查全部脱敏暂存文件后发布到明确授权仓库。远端当前为空，不需要 unrelated-history merge；不强推、不改可见性、不自动打 tag。
- 排除 SDK/二进制/本地配置/私有原件，保留开发日志；项目许可证待选择，保留依赖许可，不上传无再分发授权的论文全文。

## ADR-012 — 以单一硬件引用验证生产 RadioDriver

- 2026-11-12 接受，iteration 009；固件 0.2.0-alpha.1 / 协议 1 不变。先完成只读审查/方案，见英语 RADIO_DRIVER_AUDIT.md。
- RadioDriver 保留状态、长度校验、deadline 和事件/错误优先级；Llcc68Hardware 唯一拥有 Module/LLCC68/SPI/GPIO/ISR。RadioHardware 是非拥有引用，Runtime 先构造唯一 adapter 再构造 driver；无堆所有权、factory/task 或第二种 radio。
- RadioTransport 应用接口不变。Native 编译实际 driver.cpp，有界 fake 仅注入返回码、IRQ 锁存/电平/flags、payload 和显式 uint32 时间，不复制 driver 状态机。生产编译用 static_assert 核对固定 status/raw IRQ 值与 RadioLib 7.8.1 一致。
- 固定 PHY/RX 参数和只置 flag 的 ISR 原样移动。保留 HEADER_VALID 条件、timeout 优先级、再次 begin 失败后既有 initialized 标志，以及 read 成功但 finishReceive 失败时保留的 metrics；不改变恢复策略。
- Host 不执行真实 adapter/SPI/BUSY/ISR 并发或 RadioLib 内部；同步等待/分配、NVS/RF 实物门槛仍未验证，不宣称完整通用 PHY/平台服务。

## ADR-013 — 通过五个存储操作验证生产配置策略

- 2026-11-12 接受，iteration 010；先审查/方案后改源码，见 CONFIG_STORE_AUDIT.md。从已核实的 Iteration 009 remote HEAD 创建分支，不从后来合并的 main 开始。
- ConfigStorage 只有 begin/getBytesLength/getBytes/putBytes/end；NvsConfigStore 保存非拥有引用，单一 PreferencesConfigStorage 拥有 Preferences 并原样转发 namespace/key/返回值，无配置策略。Runtime 先构造 adapter 后 store；只支持现有串行非重入调用，每次成功 begin 显式 end 后才重开 readback。
- 实际 store 保留验证、编解码、错误字符串/顺序和读回比较；native 执行生产 store/DeviceConfig。有界 fake 只有存储字节、返回计划和调用轨迹，不复制 schema/codec/load/save 模型；无堆所有权、factory/文件系统/通用框架。
- 数值 namespace 初始化表达式保留在 NvsConfigStore 且逐字节不变，不解码历史标签。device key、schema 1、24-byte LE/FNV 格式、验证规则不变；固件 0.2.0-alpha.1、协议 1、RadioLib 7.8.1、radio/app/PHY/console/runtime dispatcher 受保护。
- 保存失败不承诺回滚或持久性；readback 失败原样传播 load 错误，不一律改为 verify。有效但不同配置仍为 nvs_verify_failed。失败 load 不改调用方配置，运行配置仍由原重启流程生效。
- Native 仅验证模拟响应下的 store 策略，不验证真实 Preferences/NVS/Flash commit、断电、重启/损坏恢复、磨损/保持时间。防御性 encode-false 分支保留但不虚称注入覆盖；依赖等待/分配和硬件门槛保留。

## ADR-014 - 用 native SDK 适配执行唯一生产协调器

- 2026-11-12 接受，iteration011；先审查/方案后修改，见 RUNTIME_AUDIT.md。固件0.2.0-alpha.1/协议1不变。
- FirmwareRuntime 接受现有 ConfigStorage/RadioHardware 引用，仍拥有实际 store/driver/console/logger/可选 role apps；无新 lifecycle/platform 接口、factory/task 或第二协调器。Esp32Firmware 先构造具体 Preferences/LLCC68 owner，再构造唯一 runtime，生命周期覆盖引用；main 15行仅委托。
- runtime.cpp/freshBootId 策略原样保留，仅换 C++ cinttypes 兼容旧 host；扩展 native Arduino/esp_timer 适配，提供有界串口输入、micros、reset/reason/raw random。同一生产 app/store/driver/codec/logger 执行；这些 header 不是硬件模型或通用生产 MCU 端口。
- 固件使用标准 optional；已安装 GCC6.3 缺少该 header 时由项目 namespace alias 选择 experimental optional，仍原地 emplace/bool/dereference，无 namespace std 改写或堆所有权。复现 ERR-017 后在 native 全局启用 UNITY_SUPPORT_64，保留完整64位断言，不截断/删除。
- 保留既有策略：app update 先于串口解析，未 init 的 poll/stop 不访问硬件，save 仅暂存直到新启动，stop失败仍可 reset，100ms捕获时间 deadline 不等待日志排空，以及非零/previous-ID 拒绝循环；随机源失败活性/跨启动碰撞未解决。
- 低层响应+实际协调器仅证明软件顺序；对象重建/虚拟reset/raw random/串口容量不证明真实 NVS/重启/USB/熵/SPI/IRQ/RF，不增加后续 MAC 或外部集成。
