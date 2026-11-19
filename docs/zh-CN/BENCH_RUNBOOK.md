# M0–M2 Heltec 实物流程

本流程面向用户报告的两块 V3.4。先读 [英文完整流程](../en/BENCH_RUNBOOK.md) 和 [硬件参考](HARDWARE.md)。本轮未做实物操作。

1. 核对到货PCB/RF/芯片、IPEX1.0、数据USB-C线和稳定5 V。上电前连接正确50欧姆负载或天线；单板也有集成radio，未知factory固件/保存角色可能TX。
2. 识别PRG/RESET、CP2102端口，在ROM模式做同板两次一致全Flash私有备份、hash和恢复计划，核对实际8 MB/security/分区。安全/加密/读取限制不明时停下，不改eFuse、不force。备份不上传，不跨芯片恢复完整镜像。
3. 核对上传图片/擦写范围；当前build的NVS0x9000/0x5000、app0x10000不等于读过实板。SDK可能在Runtime前格式化NVS（ERR-023），备份先于首次app boot。任何NVS擦除/损坏fixture都须有明确操作意图、正确offset和恢复证据。
4. 执行 `pio run -e heltec_wifi_lora_32_V3`、`pio test -e native` 和PlatformIO Python的 `python -m unittest discover -s tools -p test_capture_bench.py -v`。核对端口后，未来上传命令为 `pio run -e heltec_wifi_lora_32_V3 -t upload --upload-port COM_BOARD`，监视命令为 `pio device monitor --port COM_BOARD --baud 115200`。本次没有执行上传/monitor。CP2102/UART0为默认，原生CDC关闭；开口/DTR/RTS仍可能reset。
5. 单板始终接RF终端，不能套用过去“拆掉外置radio”的单MCU流程。missing/invalid应进入provisioning/radio disabled且无普通TX；已有有效角色则初始化SX1262，按实际结果记录。
6. 显式配置Gateway network/node/gateway=1/1/1、Client=1/2/1；命令为help、status、config show、config set role gateway|client、config set network-id、node-id、gateway-id、hello-interval-ms 15000、config save、reboot。保存验证/读回，只有重启激活；核对power-cycle、无效ID/角色关系、解析恢复。拒绝无效staging不等于实板损坏NVS测试。
7. 确认radio init状态和驱动选择，测供电、TCXO/BUSY/DIO1/reset、串口稳定和实际复位。驱动名字不是芯片ID实测；卡住时保留失败日志、安全断电，不假定默认watchdog解决ERR-021。
8. Gateway先启动，再Client。两端使用同一获准镜像、BW125、SF7、CR4/5、preamble8、CRC/explicit、sync0x12、10 dBm、TCXO1.8 V、DC-DC。记录实际允许操作条件和天线/feed，不用改PHY掩盖供电问题。
9. 关闭monitor再capture。未来命令：`python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 3600 --out logs/test/RUN-YYYYMMDD-HHMMSS`；单板改 `--board COM_BOARD --seconds 180`。使用PlatformIO Python，新目录及RUN_RECORD模板。默认开始/结束status和尾读不是完整性保证；不能用reset后的零计数代替结束计数。
10. 要求COMPLETE、无error、raw length/hash/offset一致、boot边界明确、相关log_dropped=0。闭合cohort至少200次HELLO完成T，另报全部API尝试A（含start失败）、gateway唯一接收R、duplicates、ACK开始/完成和接受C。PDR=|R intersect T|/|T|，success=|C|/|A|，零分母N/A。按LOGGING_GUIDE关联Gateway头身份与Client echo，不以host时间代替RF时序。
11. 99%短距transaction成功只是初始工程目标，不是现场保证。记录RSSI/SNR/时延/复位/供电和排除项，做受控失收timeout及恢复后新事务。M2无自动重传；真实重复/错身份/坏帧和corrupt-config需要独立fixture，native不能替代。

M0核实板/供电/USB/Flash/天线/RF条件，M1核实两角色radio/IRQ/BUSY/NVS/串口，M2保留正常和负面测试证据；ERR-021恢复仍单独开放。未知填NOT MEASURED，M2未完成前不开始M3。


## V3.4 和 OLED 补充检查

记录板 alias、PCB 正反标记、RF SKU、MCU/radio 标记、天线/接头、端口 alias、flash、
供电及固件 SHA-256。核实 Vext 低有效、17/18/21、SSD1306 和 0x3C 后方可试用
`pio run -e heltec_wifi_lora_32_V3_oled`。默认环境用于关闭显示对照。用适当负载检查
provisioning、RADIO FAULT 和 WAIT。串口 status 显示 OLED 结果及 I²C/刷新耗时最大值。
在相同获准条件比较 OLED 开关两组的供电、重启、log drops、ACK 和事务耗时。

915 MHz 是现有研究设置，在印尼 LPWAN 920–923 MHz 参考范围之外。先核对频段、
设备许可、EIRP/天线与占空比，获准后才辐射。对应法规见英文流程。SF7/BW125 下，
HELLO 28 字节约 66.816 ms、ACK 37 字节约 82.176 ms。单对每 15 s 的估算分别为
0.44544% 和 0.54784%。另计重启、重复 ACK 和其他发射。两端配置同样间隔，Gateway
用该值判断新鲜度。15 s 候选间隔不改变编译频点，也没有固件占空限流。

Gateway 先、Client 后，约 3 m 起步，单极天线竖直；后续可测 10/25/50/100/200/400 m。
保持 PHY、功率、天线、高度、极化和时长一致。独立主机使用各自 `--board`，两边
匹配 run ID 并区分后缀。15 s 间隔下 1200 s 不够 200 次，至少预算 3000 s 加事务与
开头/结尾余量，核对闭合 cohort。按方向报告 RSSI/SNR 分布、有效样本数、最小、中位、
p95、最大值，同时分列 ACK 开始/完成/接受、错误、日志丢失和重启。缺串口行不能当作
射频丢包，屏幕照片只能辅助原始串口记录。

## ERR-021 计划注入流程，尚未执行

先通过正常 M0/M1，再另审电气 fixture，现场有人值守。不得将 SX1262 BUSY 输出
短接电源或与其对驱。集成板必须经实际 PCB 审查建立有缓冲、隔离的 MCU 输入路径；
不能避免电气冲突时暂缓注入，不凭软件说明剪线或驱动输出。

1. 保留同板备份、安全状态、分区、固件 hash 和初始 NVS；接合适 50-ohm 负载，
   使用限流供电及独立断电，开测前约定并记录外部最长持续时间（TBD，非射频实测上限）。
2. 用正确电平、高阻仪器观测 NSS/SCK/MOSI、BUSY、DIO1、reset、供电/电流和 UART，
   先采正常 TX。仅见 setTx opcode 不证明返回成功。
3. 经过审查的 trigger 必须让 MCU 在 setTx 返回成功后、独立 ramp poll 前/期间看到
   BUSY 高。提前拉高可能只触发有 timeout 的 SPI 路径，不算 ERR-021 复现。
   trigger 时延和隔离需实测，目前没有已提供/已执行的 fixture。
4. 记录 loop/UART 停滞、持续时间、DIO1/RF/供电及 watchdog/reset；调试器会改变
   watchdog，须披露。到外部上限即停止，必要时安全移除全部供电，不无限等待 poll。
5. 去除故障，核对接线/供电，按同板备份恢复规则处理状态，再测初始化和 HELLO/ACK。
   失败、恢复 capture 均保存。

关闭该问题需要精确故障复现及经审查的有界返回或独立测量的恢复，包含 PA 停止与
重新初始化。正常链路或源码审查不能关闭 ERR-021。本迭代未改库/watchdog，未做实物注入。
