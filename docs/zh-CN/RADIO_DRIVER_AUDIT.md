# Iteration 009 — RadioDriver 故障路径审查

英语 RADIO_DRIVER_AUDIT.md 为完整基线/状态/错误优先级记录。本文件为对应中文摘要。

## 修改前审查

修改前 local main/fetched origin/main 均为 `b040ef111b82db96ab81a3c7377efbcf46385221`，工作树干净，origin 为明确指定仓库。创建 `iteration-009-radio-driver-verification`；不合并 main，不创建 PR。固件 0.2.0-alpha.1、协议 1、RadioLib 7.8.1 不变。已读取工程记录、driver/transport/runtime、PHY/pins/native 基础和已安装依赖实现；先记录方案，再改源码。

原 driver 有 OFF/IDLE/RX/TX/FAULT 状态；begin 顺序为 SPI/LLCC68、explicit header、CRC 2、安装 ISR；首个错误短路。receive 在未初始化/TX 时拒绝，否则开始 RX，失败进 FAULT。transmit 保留原 PACKET_TOO_LONG 参数/状态拒绝码，先 standby 后发送；TX deadline 为 now+2,000,000 us。stop 成功为 IDLE，失败为 FAULT；未初始化时无硬件调用。

poll 的软件 TX deadline 优先于 IRQ；完成/timeout 的 finishTransmit 错误优先。RX 先 standby 固定 buffer，TIMEOUT 优先于 CRC，再读长度/数据。CRC_ERR 始终失败；HEADER_ERR 只在无 HEADER_VALID 时失败。长度只允许 28..96。readData 成功后才读取 RSSI/SNR；finishReceive 失败覆盖事件/code 并进入 FAULT，但保留已成功读到的数据/metrics。再次 begin 失败保留此前 initialized=true。以上现有行为不改。

依赖 readData/finishTransmit/finishReceive、getIrqFlags/getPacketLength、IRQ 定义和 LLCC68 begin 已检查。无单独状态返回的 getter 不能虚构错误码。原 native filter 排除 driver，app fake 只提供完成事件，因此不能覆盖 driver 分支。

## 最小 seam 和结果

RadioDriver 持有 RadioHardware 非拥有引用，保留全部状态/校验/优先级。Llcc68Hardware 移入唯一 Module/LLCC68/SPI/GPIO/最小 ISR 和原固定参数；Runtime 先构造 adapter，再构造 driver。应用 RadioTransport 不变，无 heap/factory/tasks/第二 radio adapter。生产 static_assert 核对 status/raw IRQ 与 pinned RadioLib；native 编译实际 driver.cpp，fake 仅提供返回码、IRQ 锁存/电平/flags、固定包存储和调用轨迹，不复制状态机。

- `pio run -e esp32c3_supermini`：退出 0，PASS，147.15 s。
- `pio test -e native`：退出 0，163/163 PASS、0 失败/跳过，26.04 s。
- 既有 92 个测试全部通过（app 38、codec 18、config 13、console 10、sequence/timing 7、logger 6）；新增 driver 71 个全部通过。
- 新增组别：初始化 8、receive 5、transmit/airtime 10、poll/IRQ/TX completion 11、deadline/wrap 8、RX/data/metrics 22、stop/repeated 7。
- 覆盖 0..27 全部短长度、97/256/SIZE_MAX 超长、28/96 有效边界；失效/恢复、无 IRQ/无关 flags、CRC/Header 条件、timeout/清理优先级、deadline 前/恰好/之后和 uint32 wrap；32 对重复 RX/TX。-701/-702 是合成错误，非真实观测码。

逐条 case/PASS 结果：`logs/test/iteration-009-scenarios.md`；完整输出 `logs/build/iteration-009-esp32.log`、`logs/test/iteration-009-native.log`，仅脱敏机器路径。未测量行/分支覆盖率，未声称 sanitizer/并发证明。

## 保留性及剩余门槛

未复现需要改变行为的生产缺陷。driver 在明确转发/常量替换后全部条件、state、deadline、调用顺序与基线一致；34 个保护文件逐字节相同（main.cpp 15 行，app/runtime 实现、NVS、protocol、transport、pins/PHY/版本）。仅 native filter 增加 driver.cpp。初始化/RX/ISR 参数原样移动。见 `logs/build/iteration-009-source-review.json`、`iteration-009-source-manifest.sha256`。

RAM 24,532、flash 363,730 bytes，较 iteration 008 增加 16/264；引用虚调用会改变二进制布局/可能影响时序，不声称二进制或实测时序相同。新 seam 无显式堆分配，库分配/同步等待限制保留。

Host 不执行真实 adapter、SPI/BUSY/ISR 并发、RadioLib 内部。实物 NVS/重启/SPI/IRQ/RF/天线仍 NOT VERIFIED；库存两个模块/一个 MCU，第二 MCU/计划 Moxon 未就绪，未组装双设备。ERR-005/008/009/010 未关闭。下一软件任务建议配置存储边界验证；M0–M2 整体验收仍 IN PROGRESS，M3–M8 NOT STARTED。
