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

## 3. M1 — SX1262 初始化

分别在两台设备上验证： - RadioLib SX1262 初始化； - BUSY 和 DIO1
事件； - Reset； - 电源稳定性； - 无重复 brownout 或 watchdog reset。

配置无效时必须进入 Provisioning Mode，禁止正常无线发送。

## 4. M2 — HELLO / ACK

短距离、无遮挡、固定 PHY 测试。请求功率 22 dBm，发射前核对负载、供电和获准射频条件。

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

Iteration 019 使用模拟 radio transport 执行实际 v2 状态机。单 Client 路径和位图 1、
10、70 位置的格式测试为主机证据，下面的实物验收仍未执行。见 [v2 规范](SCHEDULED_LINK.md)。

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

## 12. Iteration 010 — 实际配置存储边界

`pio test -e native` 退出 0，219/219 PASS（新 store 56、既有 regression 163），ESP32 编译退出 0。完整日志 `logs/test/iteration-010-native.log`、`logs/build/iteration-010-esp32.log`；逐条结果 `logs/test/iteration-010-scenarios.md`。

CONFIG_STORE_AUDIT.md 映射 A–E：实际 store/config/codec 执行，fake 仅注入 bool/count/字节/session。断言 golden blob、数值 namespace/key、RO/W/RO、end/顺序、失败 caller 不变、写失败不读回、错误优先级、不同有效读回为 nvs_verify_failed。失败不保证回滚；不可达 encode-false 防御分支不虚称注入覆盖，无行/分支百分比。

真实 Preferences/NVS/Flash commit、断电/重启/损坏恢复、磨损/保持时间均未验证。Runtime boot/暂存生效只源码核对，未由此 suite 执行。无新命令、迁移、协议/PHY 或后续 MAC。

## Iteration 011 — 实际runtime生命周期

`pio test -e native` 退出0：**266/266 PASS**，新增runtime47和原219全部通过。`pio run -e esp32c3_supermini`退出0、PASS。最终完整日志 `logs/test/iteration-011-native-retry.log`、`logs/build/iteration-011-esp32-final.log`；首次native编译失败与修复（ERR-017）分开保留，未删除断言。逐条结果 `logs/test/iteration-011-scenarios.md`。

RUNTIME_AUDIT.md映射A–G：boot11、provision8、serial7、radio4、reboot10、session3、diagnostics4。实际协调器/store/driver/apps/console/logger执行，观察器断言已关闭config read先于radio、radio先于app RNG、无效启动零硬件访问。暂存save不切换已加载角色/ID，新实例才生效；虚拟deadline验证before/at/after、TX完成/超时和uint32 wrap；容量0/1与queue drop证明reset可遗留诊断；Gateway65,537次验证实际generator/session轮换，通过实际帧/日志验证角色与callback。

没有第二协调器模型，不声明覆盖百分比、熵、硬实时或实物验证。脚本只模拟返回/输入/time/reset；真实adapter/SDK/NVS/flash/USB/IRQ/RF/断电语义待验证。失败stop继续reset、app先输入与日志未排空reset均保留。下一步建议依赖时延/分配与永久坏RNG活性评估，不启动M3–M8。

## Iteration012 — Boot-ID活性与延迟依赖

修复前：Git取得实际011内部generator，raw64 guard对三种永久坏输入有限退出，probe退出0。两次focused均9FAIL/1PASS、退出1（首次wrap fixture改Gateway隔离dueClient TX）；修复后focused10/10退出0，完整native290/290退出0（旧266不变、新24），ESP32退出0 PASS。完整失败/成功日志、53输入hash和每条结果见iteration-012证据，不虚构覆盖百分比。

真实runtime/apps/driver/store/console/logger执行，覆盖启动坏值/max/零前缀、第8次/第9次延后、重试频率/pending reboot/wrap、65536次耗尽后counter保留、Gateway三种永久坏rollover及Client失败，恢复拒绝previous100后用200/seq0。延迟read/TX finish/save/stop/serial覆盖ACK期限、reset100ms、wrap及既有尝试起点recovery/TX预算、阻塞期间延后服务。无无限loop、第二runtime/MAC模型或生产测试钩子；不声称实物堆/栈/flash/RF/熵/NVS/重启验证。

## Iteration013 - 实物验收准备（未执行）

ESP32退出0 PASS13.053s；native退出0 **290/290 PASS**61.980s，53输入/有序cases/bin与012一致，无firmware/native test/pins/PHY/依赖改动。采集工具另有13/13 fake-port检查，不并入native或RF；修正前实际旧工具中断false-success用fake复现，无设备。完整日志/manifests/validation在iteration-013证据。

流程/门槛见PHYSICAL_ACCEPTANCE_READINESS、BENCH_RUNBOOK、HARDWARE、RUN_RECORD_TEMPLATE与LOGGING_GUIDE CSV。未开口/烧录/擦除/复位/RF，no-port枚举不是物理不存在证明。单MCU须先同板备份/安全/layout/恢复，SDK boot前置格式化风险ERR-023需backups；暂存invalid不等于corrupt-NVS boot。合法Gateway无radio仍init，可能fault/block。Pair M0电气/RF、M1 IRQ/BUSY/恢复/no unintendedTX、M2>=200完成cohort及A/T/R/ACK/C分母全待证据。replay/错身份/corrupt-NVS fixture未提供/执行；ERR-021阻挡无人值守/卡BUSY恢复。不用host时延或碎日志代替实物，不改watchdog。

## Iteration014 - Heltec V3.2 HF迁移

用户后续授权实施。S3编译通过，原290用例名称/顺序保留且通过，新增实际adapter12，总302；capture另13为fake-port。旧测试仅driver标签断言更新。首次fixture编译失败及修正记录保留，不声称行/分支coverage或物理验证。

新suite只用有界SPI/RadioLib调用记录，验证pins/TCXO1.8 V/DC-DC/固定PHY、初始化和错误顺序、IRQ latch/level、RX mask、字节/指标转发；真实库常量/variant由S3编译检查。实物按当前集成radio流程：上电前终端、V3.2 HF/CP2102核对、同板ROM备份/security/layout、provisioning、供电/TCXO/BUSY/DIO1/reset/串口/NVS、至少200闭合cohort及负面fixture。ERR-021/023仍开放，M0–M2实物未完成，M3–M8未开始，不能把新板当5/70节点容量证据。


## Iteration 015 — 实物前边界与分析工具

S3 构建退出 0；native 307/307 退出 0，无跳过，原 302 名称/顺序保留。
五项新测试执行真实 tracker：1/5/8 保留、9/10/70 淘汰与顺序重放、boot churn
挤占其他 Client、多 Client rollover/新旧 session、Gateway 对象重建。
逐案例结果见 logs/test/iteration-015-scenarios.md；未增加自动重传、DATA 交付或
持久 replay 保护，不证明 70 Client 可靠性。

另有 capture 13/13、model 16/16、consistency 7/7，全部退出 0。模型覆盖手算 airtime、
CRC/header/CR/preamble/LDRO、异长 payload、bitmap 完整开销、每轮 ACK、未知成本、
预算边界、输入拒绝和 CLI。文档测试注入错版本/环境/链接/实物 PASS/归档标识。
前两次 checker 失败保留在 verification/corrected 日志（ERR-026），final 全通过。
216 分析场景中 103 INFEASIBLE、113 UNDETERMINED，不是无线/包级仿真。
假设与候选合同见 TDMA_FEASIBILITY；guard/切换/处理未知，不报告实物 capacity。
line/branch 百分比、物理时序、heap/stack、NVS 耐久和 RF 均未测量。
收到板和安全审查 fixture 后再按 runbook 执行，不能用主机结果关闭实物门槛。


## Iteration 016

默认和 OLED 候选两种 S3 构建分别验证。Native 用模拟 GPIO/I²C/时钟与 radio 执行实际
Runtime、应用和显示逻辑，覆盖初始化顺序、分块、变化页、计数、缺失指标、新鲜度与
故障隔离。同一 HELLO/ACK 在显示关闭、启用和故障的模拟条件下保持一致。实板要确认
V3.4 引脚、RF SKU、供电及备份，再做 OLED 开关对照和获准短距采样。验收条件每组至少
200 次完整 HELLO TX，并留原始串口、两个方向指标、OLED 时序、日志丢失和复位结果。
