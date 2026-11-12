# 测试计划

## 1. 测试层级

必须区分： 1. 静态代码检查。 2. Host 单元测试。 3. ESP32 编译。 4.
单设备无线初始化。 5. 双设备射频测试。 6. 多节点仿真。 7.
多节点实物测试。

某一层级通过不代表更高层级通过。

## 2. M0 — 硬件和工具链

记录开发板版本、模块标记、GPIO
连通性、电源电压、天线接口、天线方向、PlatformIO、Arduino core、编译器和
RadioLib 版本。

部署前还需确认当地允许的频率、信道和功率要求。

## 3. M1 — LLCC68 初始化

分别在两台设备上验证： - RadioLib LLCC68 初始化； - BUSY 和 DIO1
事件； - Reset； - 电源稳定性； - 无重复 brownout 或 watchdog reset。

配置无效时必须进入 Provisioning Mode，禁止正常无线发送。

## 4. M2 — HELLO / ACK

短距离、无遮挡、固定 PHY 测试。初始发射功率 10 dBm。

至少记录 200 次 HELLO： - 尝试数量； - Gateway 接收数量； - ACK
发送数量； - Client 成功确认数量； - Timeout； - Duplicate； -
RSSI/SNR； - Reset 和 Driver Error。

短距离测试可将 ACK 成功率不低于 99%
作为初始工程目标，但这不是现场性能保证。未达标时应先分析日志，不要同时修改多个射频参数。

## 5. Native 单元测试

第一阶段必须测试： - 正常帧编解码； - 字节布局和端序； - 最小/最大
Payload； - 空 Payload； - Header/Payload 截断； - 多余字节； - 错误
Magic； - 不支持的协议版本； - 未知消息类型； - 错误 Header 长度； -
保留字段错误； - Network/Destination 错误； - ACK 身份回显； - Sequence
wrap； - 重复包识别； - NVS 角色配置验证。

M4 阶段增加 1、10、70 节点 Bitmap 测试。

## 6. M3–M8

- M3：单客户端 Beacon/TDMA、slot timing、radio switching 和 clock
  drift。
- M4：Bitmap、丢包注入、确定性重传、去重。
- M5：SF7/SF8/SF9 ADR、滞回和握手丢包。
- M6：Beacon 丢失、重启、重新加入。
- M7：固定位置和参数下比较参考天线与 Moxon。
- M8：仿真 5/10/20/35/70 个逻辑客户端，必须标注为仿真结果。

## 7. 测试记录

每次测试记录日期、固件版本、协议版本、Git
commit、设备角色/ID、硬件版本、PHY、天线、位置、包数量、结果、指标、日志路径和未解决问题。

没有验收证据不得将 milestone 标记为 PASS。

## 8. 2026-11-12 候选实现证据

`logs/test/iteration-003-native.log`：44/44 测试通过，退出码 0（codec 18、console 10、role/config 9、sequence/timing 7）。`logs/build/iteration-004-esp32.log`：固件编译退出码 0。两者不能替代实物证据。

Host 验证了配置编码/校验、解析器和 ACK 身份等；Preferences 持久化、断电重启、损坏/不兼容 NVS 启动、LLCC68 实物初始化及 RF 测试均未执行。按 `BENCH_RUNBOOK.md`、`tools/capture_bench.py` 和 `logs/test/RUN_RECORD_TEMPLATE.md` 记录双设备测试。捕获工具的 help/语法检查不代表实物串口捕获成功。M0–M2 验收完成后才能进入 M3。

最终 iteration-005：`logs/build/iteration-005-esp32.log` 退出码 0；`logs/test/iteration-005-native.log` 44/44 通过、退出码 0，替代此前候选二进制。HEADER_ERR/HEADER_VALID 条件已静态对齐依赖库，但未执行实物 CRC/Header IRQ 测试。

## 9. iteration-007 软件验证

实际库存两个模块/一个 MCU，第二个 MCU 尚未购买，Moxon 尚未制作，未组装双设备。只执行软件测试。

`logs/test/iteration-007-native.log`：92/92 PASS、退出码 0（生产状态机 38、codec 18、config 13、console 10、sequence/timing 7、logger 6）；`logs/build/iteration-007-esp32.log` 编译退出码 0。全部 case 见 `logs/test/iteration-007-scenarios.md`。这是场景计数，未测量行/分支覆盖率。

实际 GatewayApp/ClientApp/SerialLogger 与纯协议/config/console 编译执行；native-only 时钟/Serial/fake transport 提供刺激，不复制状态机。覆盖 HELLO/ACK 丢失、ACK 截止前/恰好/之后、错误身份/发送者/网络/目的、重复包、44 种畸形 HELLO 和 45 种畸形 ACK、TX 失败、RX 三次恢复/回退、deadline 不延长、停止新事务、对象重建、旧 session/stale/乱序、32 位计时 wrap/64 位间隔、200 次重复及独立 65,537 次完整序号耗尽事务。

不新增自动重传；测试重放完全相同字节并验证不重复接受。超时后新事务用新序号。不声称 Runtime 重启调度、真实随机源、NVS/Flash、RadioDriver poll/IRQ/SPI/BUSY/硬件 CRC、RF/USB 延迟已验证。物理门槛保持 IN PROGRESS。

## 10. iteration-008 独立身份验证

ESP32 编译退出码 0、PASS（`logs/build/iteration-008-esp32.log`）；native 92/92 PASS、0 失败/跳过、退出码 0（`logs/test/iteration-008-native.log`），38 个 integration 与原场景相同。35 个 source/header/test 的静态等价检查通过，版本、wire/PHY、状态语句、NVS 调用及旧 namespace 字节均保留。

发布检查涵盖身份、凭据、私有路径/设备标识、双语 README 事实与链接、暂存清单/内容、依赖及工件。先建立审查清单再编辑；旧记录仅脱敏并明确 hash 来源，不删除失败/解决历史。实物 NVS/重启/SPI/IRQ/RF/天线仍 NOT VERIFIED；没有后续 MAC、存储迁移或新 radio adapter 验证。

## 11. Iteration 009 — 实际生产 driver 故障路径

Native 通过 RadioHardware 引用执行实际 RadioDriver：新增 71/71 PASS，既有 92/92 PASS，总计 **163/163 PASS**、退出 0（`logs/test/iteration-009-native.log`）；ESP32 编译退出 0（`logs/build/iteration-009-esp32.log`）。逐条名称/结果见 `logs/test/iteration-009-scenarios.md`；A–G 映射见 RADIO_DRIVER_AUDIT.md。

Fake 仅注入有界 bytes/返回码、IRQ 锁存/电平/flags 和显式 uint32 时间，无 driver 状态模型。覆盖 deadline 前/恰好/之后、wrap、长度/错误/恢复和优先级；保留 read 成功但 cleanup 失败时有 metrics 且报 ERROR/FAULT 的既有行为。Host 不验证 adapter/library/SPI/BUSY/真实 ISR 时序；无行/分支百分比或实物结果声明，协议/app/NVS/pins/PHY/版本不变。
