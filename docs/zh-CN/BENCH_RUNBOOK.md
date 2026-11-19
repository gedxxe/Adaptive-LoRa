# M0–M2 Heltec 台架流程

本流程适用于两块 Heltec WiFi LoRa 32 V3.4 与原装单极天线。用户确认采购选项为
902–928 MHz，V3.4 沿用 V3.2 电气线路及引脚。以 V3.2 原理图为参考，按板分别记录
实物结果。完整的 Windows 和 ROM 命令见 [英文流程](../en/BENCH_RUNBOOK.md)。

## 1. 识别板卡和 Windows COM 口

断电标记 Gateway 与 Client，查看每板 RF 接头和天线。先接合适的 50 欧姆负载，再用
稳定 USB 5 V 上电。工厂固件或已保存角色可能发射，备份、上传和配置期间保持终端。
采用已知可传数据的 USB-C 线，直连电脑，不经集线器。每次只连接一块，比较插入前后
的设备管理器和 `pio device list`。首次上电需保留工厂配置时，按板上的 PRG/BOOT 步骤
进入 ROM 下载模式，避免启动应用。先记录并备份这块板，再操作另一块。

1. 在 **端口（COM 和 LPT）** 查找 CP210x USB 转串口桥和新增的 COM 号，拔插比对确认。
   蓝牙虚拟串口与板卡无关，重新连接后 COM 号可能变化。
2. 没有新 COM 时，查看 **通用串行总线设备** 和 **其他设备**。从属性的详细信息和硬件
   ID 记录 VID/PID、设备描述、COM 号及板 alias。
3. 若 USB 能枚举但提示缺驱动，安装 **Silicon Labs CP210x Universal Windows Driver**
   后重新连接。在 [官方驱动页面](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers)
   获取驱动。完全没有新设备时先换数据线和直连端口。
4. 用 `pio device list` 核对对应 COM。没有对应端口即停下，不能猜号或开始上传和采集。
   串口打开可能因 DTR/RTS 导致复位，备份前不打开 monitor。

## 2. 同板备份及 flash 安全/布局

先做手动 ROM 下载模式操作，保持 RF 终端。按英文命令用 esptool 5.x 的 `get-security-info`
和 `flash-id` 核对安全状态与容量。仅在允许正常读备份且确认 8 MB 后，读取两份完整
flash，检查 8,388,608 字节和 SHA-256 一致。按照实际分区保存 NVS bytes 及同板恢复计划。
核对目标镜像写入范围与实际布局。构建参考 NVS 0x9000/0x5000、app0 0x10000 不代表实板
布局。ERR-023 说明 SDK 在应用 runtime 之前可能格式化 NVS。备份留在仓库外，只能回到
原板。遇到限制、布局不明或读失败时停止，不强制、不擦除、不改 eFuse。

## 3. 核对外设并上传 OLED 候选

记录实物 SKU、PCB 标记、RF 接头、天线、供电、flash 和 USB。检查 SDA/SCL/reset、Vext
极性和稳定时间，以及 SSD1306 0x3C 参考。完成备份、布局和外设核对后，记录镜像哈希，
对两板使用同一候选镜像。每次关闭其他串口工具，把 `COM_BOARD` 换成实际端口。

```powershell
pio run -e heltec_wifi_lora_32_V3_oled
Get-FileHash -Algorithm SHA256 .pio/build/heltec_wifi_lora_32_V3_oled/firmware.bin
pio run -e heltec_wifi_lora_32_V3_oled -t upload --upload-port COM_BOARD
pio device monitor --port COM_BOARD --baud 115200
```

上传后查看 BOOT、status 和 OLED。有效旧角色可能发射，继续保持 RF 终端。默认
`heltec_wifi_lora_32_V3` 用于关闭显示的对照。

## 4. 分别配置角色

以 115200 baud 先查看 `status`，按板输入以下命令。核对暂存值并确认 `CONFIG_SAVED`
后才手动重启。保存失败即停止。重启后以 `status` 核对已加载的角色、ID、schema 1 和
15000 ms。

Gateway：

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 15000
config show
config save
reboot
status
```

Client：

```text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config set hello-interval-ms 15000
config show
config save
reboot
status
```

两板均为 15 s。Client 每次事务结束后等 15 s。Gateway 使用该间隔判断屏幕新鲜度。
WAIT 表示尚无有效接收。ONLINE 表示 Gateway 收到有效 HELLO 或 Client 接受 ACK 后
不足 31 s，不能说明持续连接。达到 31 s 则显示 STALE。联通前查看 RADIO_INIT、
OLED_INIT、status 和复位边界。

## 5. 先做双板联通，再扩展距离

915.0 MHz 是固定暂定研究频点。辐射测试须使用获准的射频配置。Gateway 先启动，Client
随后，约 3 m，原装单极天线竖直。关闭 monitor，使用实际 COM 和新目录采集。

```powershell
python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out logs/test/RUN-YYYYMMDD-HHMMSS-smoke
```

使用 PlatformIO Python，保存原始串口、chunk 索引、元数据、OLED 状态及
[记录模板](../../logs/test/RUN_RECORD_TEMPLATE.md)。确认 HELLO 完成、Gateway 接收、ACK
开始和完成、Client 接受的回显身份、双向 RSSI/SNR，以及稳定复位边界和无丢日志。联通
或采集不可靠时保留证据并停止测距。

在相同几何、供电、PHY 和间隔下比较 OLED 开关，查看 PDR、ACK 成功、事务时长、复位和
日志丢失。记录 OLED_STATUS 的单次 I²C 和刷新 I/O 最大时长。可用高阻探头记录 DIO1、
SCL、radio NSS，比较 DIO1 到首次 SPI 服务与 ACK 启动时延。OLED 耗时不等于 IRQ 延时。
联通后才扩展 10/25/50/100/200/400 m。每个选定验收条件至少 200 次完整 HELLO TX，15 s
间隔建议预留 3600 s 并实际核对包数。独立主机使用 `--board` 和同 run ID 不同后缀。
用 [日志规范](LOGGING_GUIDE.md) 统计 PDR 与成功率，缺失串口行不能当作 RF 丢包。
实物 NVS、链路、异常和恢复证据齐备后才完成 M0–M2，M3 还需实测时序。

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
