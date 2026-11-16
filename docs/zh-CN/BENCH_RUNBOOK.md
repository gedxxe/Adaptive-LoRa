# M0–M2 Heltec 实物流程

本流程未执行，唯一目标为V3.2 HF同版两台板，详细步骤和命令以英文BENCH_RUNBOOK为准。先读HARDWARE和迁移报告。

1. 核对到货PCB/RF/芯片、IPEX1.0、数据USB-C线和稳定5 V。上电前连接正确50欧姆负载或天线；单板也有集成radio，未知factory固件/保存角色可能TX。
2. 识别PRG/RESET、CP2102端口，在ROM模式做同板两次一致全Flash私有备份、hash和恢复计划，核对实际8 MB/security/分区。安全/加密/读取限制不明时停下，不改eFuse、不force。备份不上传，不跨芯片恢复完整镜像。
3. 核对上传图片/擦写范围；当前build的NVS0x9000/0x5000、app0x10000不等于读过实板。SDK可能在Runtime前格式化NVS（ERR-023），备份先于首次app boot。任何NVS擦除/损坏fixture都须有明确操作意图、正确offset和恢复证据。
4. 执行 `pio run -e heltec_wifi_lora_32_V3`、`pio test -e native` 和PlatformIO Python的 `python -m unittest discover -s tools -p test_capture_bench.py -v`。核对端口后，未来上传命令为 `pio run -e heltec_wifi_lora_32_V3 -t upload --upload-port COM_BOARD`，监视命令为 `pio device monitor --port COM_BOARD --baud 115200`。本次没有执行上传/monitor。CP2102/UART0为默认，原生CDC关闭；开口/DTR/RTS仍可能reset。
5. 单板始终接RF终端，不能套用过去“拆掉外置radio”的单MCU流程。missing/invalid应进入provisioning/radio disabled且无普通TX；已有有效角色则初始化SX1262，按实际结果记录。
6. 显式配置Gateway network/node/gateway=1/1/1、Client=1/2/1；命令为help、status、config show、config set role gateway|client、config set network-id、node-id、gateway-id、hello-interval-ms 5000、config save、reboot。保存验证/读回，只有重启激活；核对power-cycle、无效ID/角色关系、解析恢复。拒绝无效staging不等于实板损坏NVS测试。
7. 确认radio init状态和驱动选择，测供电、TCXO/BUSY/DIO1/reset、串口稳定和实际复位。驱动名字不是芯片ID实测；卡住时保留失败日志、安全断电，不假定默认watchdog解决ERR-021。
8. Gateway先启动，再Client。同一镜像/固定915 MHz、BW125、SF7、CR4/5、preamble8、CRC/explicit、sync0x12、10 dBm、TCXO1.8 V、DC-DC。记录实际允许操作条件和天线/feed，不用改PHY掩盖供电问题。
9. 关闭monitor再capture。未来命令：`python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 1200 --out logs/test/RUN-YYYYMMDD-HHMMSS`；单板改 `--board COM_BOARD --seconds 180`。使用PlatformIO Python，新目录及RUN_RECORD模板。默认开始/结束status和尾读不是完整性保证；不能用reset后的零计数代替结束计数。
10. 要求COMPLETE、无error、raw length/hash/offset一致、boot边界明确、相关log_dropped=0。闭合cohort至少200次HELLO完成T，另报全部API尝试A（含start失败）、gateway唯一接收R、duplicates、ACK开始/完成和接受C。PDR=|R intersect T|/|T|，success=|C|/|A|，零分母N/A。按LOGGING_GUIDE关联Gateway头身份与Client echo，不以host时间代替RF时序。
11. 99%短距transaction成功只是初始工程目标，不是现场保证。记录RSSI/SNR/时延/复位/供电和排除项，做受控失收timeout及恢复后新事务。M2无自动重传；真实重复/错身份/坏帧和corrupt-config需要独立fixture，native不能替代。

M0核实板/供电/USB/Flash/天线/RF条件，M1核实两角色radio/IRQ/BUSY/NVS/串口，M2保留正常和负面测试证据；ERR-021恢复仍单独开放。未知填NOT MEASURED，M2未完成前不开始M3。
