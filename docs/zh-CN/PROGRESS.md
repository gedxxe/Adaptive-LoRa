# 开发进度

Iteration 009 软件检查 PASS：ESP32 编译退出 0（147.15 s），native **163/163 PASS**、退出 0（26.04 s）：新增实际生产 RadioDriver 故障用例 71 个，既有 regression 92 个全部通过。修改前审查/覆盖见 RADIO_DRIVER_AUDIT.md，逐条结果 `logs/test/iteration-009-scenarios.md`。分支 `iteration-009-radio-driver-verification`，此检查点 commit/push 待最终审查；不合并 main、不创建 PR。实物仍 NOT VERIFIED。

项目：**Adaptive-LoRa**。研究标题：Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks。

历史 Iteration 008 建立独立仓库并按授权脱敏历史身份/路径，保留时间顺序、结果及错误解决状态。该轮 ESP32 编译 PASS、退出码 0（`logs/build/iteration-008-esp32.log`）；native 92/92 PASS、退出码 0（`logs/test/iteration-008-native.log`），仍为 38 integration 场景。固件 0.2.0-alpha.1、协议 1、PHY、packet/state 和旧存储字节均不变；四个架构目标与缺口只记录，不新增实现。

Git 发布已核实：main 已推送到明确授权的公共仓库，建立提交 `1d7ab54379221dff8b4eb9347833282657b6e1dc` 与远端 main 一致（git ls-remote 退出码 0）。远端原为空，无需合并历史；暂存与安全审查通过。后续仅文档回执提交记录这一已核实 SHA，见英语 BUILD_MEMORY.md 和 logs/build/iteration-008-publication-receipt.json；不声称实物验证。

历史结果（iteration 007）：ESP32 编译退出码 0，native **92/92 PASS**（实际生产状态机 38、codec 18、role/config 13、console 10、sequence/timing 7、logger 6）。首轮旧 MinGW PRI 宏问题（ERR-011）已通过 `<cinttypes>` 修复。实际库存两个模块/一个 MCU；第二个 MCU 尚未购买、Moxon 尚未制作，未组装双设备。Host 场景不是实物 NVS/RF 验证；没有完整 milestone PASS。历史记录保留。

状态：`NOT STARTED`、`IN PROGRESS`、`BLOCKED`、`PASS`。只有记录验收标准和证据后才能标记为
PASS。

## 当前基线

| 项目              | 当前值                                         |
|-------------------|------------------------------------------------|
| 固件架构          | 一份固件，启动时从 NVS 读取角色                |
| MCU 库存          | 1 个 ESP32-C3 Super Mini；第二个尚未购买，未组装双设备 |
| 无线模块库存      | 2 个 RA-01SCH/LLCC68；Moxon 尚未制作 |
| RadioLib          | 7.8.1                                          |
| 频率              | 915.0 MHz，临时测试值                          |
| 初始数据 PHY      | SF7 / BW125 / CR4/5                            |
| 控制 PHY          | SF9 / BW125 / CR4/5                            |
| 初始 TX 功率      | 10 dBm                                         |
| Network ID        | 1                                              |
| Gateway ID        | 1                                              |
| Client ID         | 2                                              |
| Firmware baseline | 0.2.0-alpha.1 实现候选                         |
| Protocol version  | 1                                              |

## Milestone 状态

| ID  | Milestone                          | 状态        | 证据                   |
|-----|------------------------------------|-------------|------------------------|
| M0  | 硬件、引脚、电源、天线和工具链确认 | IN PROGRESS | 工具链已记录；实物检查待完成 |
| M1  | 两台设备 LLCC68 初始化             | IN PROGRESS | 固件编译通过；NVS/无线实测待完成 |
| M2  | 固定 PHY HELLO/ACK                 | IN PROGRESS | 163 个 native 测试通过，含生产 app/driver；RF 待完成 |
| M3  | Beacon 和单客户端 TDMA             | NOT STARTED | 等待 M2                |
| M4  | ACK Bitmap 和选择性重传            | NOT STARTED | 等待 M3                |
| M5  | SF7/SF8/SF9 ADR                    | NOT STARTED | 等待 M4                |
| M6  | 同步恢复和重新加入                 | NOT STARTED | 尚未开始               |
| M7  | Moxon 射频对比                     | NOT STARTED | 尚无受控测试数据       |
| M8  | 5–70 个逻辑节点仿真                | NOT STARTED | 尚无仿真记录           |

## 待确认事项

- [ ] ESP32-C3 Super Mini 实际版本和 Flash 容量。
- [ ] Ra-01SCH 模块标记和引脚方向。
- [ ] GPIO 连通性。
- [ ] 无线模块 3.3 V 电源及发射时电压。
- [ ] 天线接口和 50 欧姆馈线。
- [ ] 当地频率、信道和功率要求。
- [x] 实际工具链版本：Core 6.1.19，已安装 pioarduino/espressif32 55.03.311，Arduino-ESP32 3.3.11，RISC-V GCC 14.2.0 (esp-14.2.0_20260121)，RadioLib 7.8.1；host GCC 6.3.0 / native 1.2.1 / Unity 2.6.1。

历史 iteration 007 候选固件通过编译及 92 个 native 测试，版本为 0.2.0-alpha.1 / 协议 1。实际 app 状态语句未改，只改 transport 构造参数及 C++ include；main.cpp 仍 15 行。同步 NVS/BUSY、库临时分配、SDK 串口等待、IRQ/随机源假设仍未验证。硬件采购/制作/组装完成前继续软件工作，不执行实物流程，不进入 M3。详细结果及所有 case 见 SOFTWARE_AUDIT.md 与 `logs/test/iteration-007-scenarios.md`。

## 2026-11-12 实现检查点（ITERATION-001）

已添加 `0.2.0-alpha.1` 候选源码，协议仍为 1。M0/M1/M2 均为 IN PROGRESS，尚未通过实物验收。Native 测试因缺少标准库 include 编译失败（ERR-002）；ESP32 SDK 安装/编译仍在进行。没有执行烧录、NVS 实物测试或射频测试。

NVS 使用带完整性校验的 24 字节单 blob；配置编辑只影响暂存值，保存后重启生效。新增 `config set hello-interval-ms <1000..3600000>`，默认事务结束后等待 5000 ms 再发新 HELLO。角色命令不自动修改 ID；按 README 明确设置全部 ID。Network ID 可为 65535，节点地址 65535 保留。

固定数据 PHY SF7、私有 sync word 0x12；XTAL、LDO、USB CDC 和全部引脚仍需实物确认。去重缓存保留 8 个 session，每个 64 个 sequence；旧 session 淘汰和 Gateway 重启后不保留历史。状态日志报告队列丢弃计数，不能将驱动类型视为独立芯片识别。

ITERATION-002：显式 include 已修复，4 个 native suite 均已编译运行；44 个测试中 43 个通过、1 个失败。失败原因是 Unity 不允许对长度为 0 的数组执行内容比较（ERR-003），将修正该测试。ESP32 SDK 安装/编译仍在进行。未执行实物测试。

ITERATION-003：44/44 native 测试通过，退出码 0。第一次 ESP32 编译在并发头文件修改期间遇到 GatewayApp 缺少直接 include（ERR-004）；当前已补充直接 include，等待源码冻结后的编译验证。USB CDC 的睡眠相关依赖警告保留（ERR-005），本固件不请求 MCU 睡眠。发射失败与 ACK 超时分别计数；DIO1 卡高时仍受 TX 截止时间限制。未执行实物测试，M0/M1/M2 尚未 PASS。

ITERATION-004：冻结源码 ESP32 编译成功，退出码 0（`logs/build/iteration-004-esp32.log`），ERR-004 已验证关闭。仅剩 USB CDC 实物检查和全部硬件/RF 验收未完成。包列表编码/命令选项及沙箱内 compiler 探测失败记录在 ERR-006，已用 UTF-8 普通包列表/沙箱外 compiler 输出解决。只枚举到蓝牙串口，未识别 ESP32；未执行烧录或实物测量。

最终源码审查还发现 ERR-007：HEADER_ERR 必须结合 HEADER_VALID 判断，避免比 RadioLib 更严格的误拒绝；当前修正待 iteration-005 编译验证。这是静态审查发现，未声称观测到 RF 失败率。

ITERATION-005 最终结果：HEADER_ERR 条件已与固定版本 RadioLib 对齐；固件编译退出码 0（`logs/build/iteration-005-esp32.log`），native 44/44 通过、退出码 0（`logs/test/iteration-005-native.log`）。源码缺陷 ERR-007 已经静态/编译验证关闭；实物 IRQ 行为仍未测试。最终文件清单、每个 cpp 职责、SHA-256 和 M3–M8 计划见英文 IMPLEMENTATION_REPORT.md；最终源码 hash 清单为 `logs/build/iteration-005-source-manifest.sha256`。USB CDC 与全部实物门槛仍待验证，M0/M1/M2 保持 IN PROGRESS。

Iteration 008 暂存安全检查初次把 HTTPS 链接误判为本地路径（ERR-014），修正边界后检查通过，无固件修改。英语错误/构建记录保留失败及修复结果；暂存源文件与已编译清单一致，main.cpp 仍为 15 行。

最终日志检查因 Windows 默认编码读取 UTF-8 失败，显式 UTF-8 重读通过（ERR-006 再现）；编译和 native PASS 证据不变，没有固件修改。

Iteration 009 保留性：driver 仅改硬件转发/常量，状态/deadline/错误优先级不变，34 个保护文件逐字节不变；Runtime 先构造唯一 Llcc68Hardware 再构造引用注入 driver，main.cpp 仍 15 行。新 seam 无显式堆分配。RAM 24,532 / 327,680、flash 363,730 / 1,310,720 bytes。真实 adapter/SPI/BUSY/ISR 竞态、Runtime/随机源/NVS/USB/RF 不属于 native 证据，M0–M2 整体 IN PROGRESS、M3–M8 NOT STARTED。

下一软件任务建议配置存储/NVS 边界验证，使用最小 persistence fake 并保持 schema/key/blob/启动策略；driver 策略在本轮 71 场景通过，依赖等待/内存预算仍需另行证据。实物工作等待第二 MCU、天线与组装。

Iteration 009 审查：PlatformIO 日志显示尾空格/末尾空行使暂存检查退出 2（ERR-015）。仅规范化本轮公开日志的 EOL/显示空白，私有原输出保留，源码/编译/测试结果不变；修正后暂存检查退出 0，ERR-015 已验证关闭。
