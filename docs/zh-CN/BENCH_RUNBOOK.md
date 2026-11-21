# 台架测试

本流程用于首次上电备份、USB 排查和射频测量。
[主指南](../../README.zh-CN.md#烧录与配置) 提供上传和角色命令。按板卡别名记录实物
Heltec WiFi LoRa 32 V3.4 的核对结果，电气参考为 V3.2。见 [硬件说明](HARDWARE.md)。

## 1. 识别板卡和 Windows COM 口

断电标记 Gateway 与 Client，查看每板 RF 接头和天线。先接合适的 50 欧姆负载，再用
稳定 USB 5 V 上电。工厂固件或已保存角色可能发射，备份、上传和配置期间保持终端。
采用已知可传数据的 USB-C 线，直连电脑，不经集线器。每次只连接一块，比较插入前后
的设备管理器和 `pio device list`。首次上电需保留工厂配置时，按板上的 PRG/BOOT 步骤
进入 ROM 下载模式，避免启动应用。先记录并备份这块板，再操作另一块。

### Windows 设备管理器

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

## 3. 验证 v3 双板链路

上传前对照[硬件参考](HARDWARE.md)检查板卡标识、射频 SKU、连接器、闪存容量和供电。
检查显示引脚、Vext 极性和稳定时间，并核对预期的 SSD1306 地址 `0x3C`。确认天线或负载及
供电能支持固件请求的 `22 dBm`，并在获准的射频条件下测试。在试验记录中列出未确认项。

完成备份和分区检查后，按 [README](../../README.zh-CN.md#烧录与配置) 上传并配置
Gateway 1 和 Client 2。保存镜像哈希以及每块板的启动和 `status` 输出。

```powershell
Get-FileHash -Algorithm SHA256 .pio/build/heltec_wifi_lora_32_V3_multiclient/firmware.bin
```

在获准的射频条件下先做短距试验，保持天线方向和供电一致。关闭串口监视器后同时采集两端，
把占位符替换成实际端口，并为每轮使用新目录。打开串口可能导致板卡复位，应在日志中核对启动边界。
示例将原始日志写入 Git 工作区之外。

```powershell
$pioPython = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$captureDir = Join-Path $env:LOCALAPPDATA 'Adaptive-LoRa/captures/RUN-YYYYMMDD-HHMMSS-smoke'
& $pioPython tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out $captureDir
```

按 Client 身份和帧号核对 `V3_DATA_TX`、Gateway 的 `APP_PAYLOAD` 和 Client 的
`V3_PAYLOAD_RESULT`。检查确认结果、无线错误、错过的发射时隙和串口丢日志。另做一轮
Gateway 断电再上电，检查 Client 搜索 Beacon、接受新调度并恢复 DATA 交付。
状态含义见 [屏幕诊断](DISPLAY_DIAGNOSTICS.md)。

## 4. 扩展到八个 Client

按 2、4、8 个 Client 逐步扩展。对比试验保持功率、天线、载荷长度和位置一致，并按声明
的负载重复采集。对额外 Client 分别用 `--board COMx` 采集到独立目录，记录共享的试验
编号和不同的板卡别名。提前关闭各端口的监视器。

- 保存 [试验记录](../../logs/test/RUN_RECORD_TEMPLATE.md)、镜像哈希、配置、天线位置、
  供电和原始日志。
- 统计实际发送与接收次数，记录首轮交付、重传、ACK 一致性、双向 RSSI/SNR、超时和
  重新同步时长。串口丢行或缺失标为证据不全。匹配规则见 [日志规范](LOGGING_GUIDE.md)。
- 使用高阻探头测 DIO1、NSS 和 SCL，采集 DIO1 到首次处理、启动发射和恢复接收的时延。
  调整 50 ms 保护间隔前，连续多个超帧测量时钟漂移。
- 在 Gateway 日志上运行 [离线 ADR 工具](SCHEDULED_LINK.md#链路自适应实验)，对照 Client
  记录后再设计切换实验。

## 旧版 M0–M2 验收

需要对照 v1 HELLO/ACK 时，使用 `heltec_wifi_lora_32_V3`，核对 `profile=m2`
和 `protocol=1`。M2 仍需板卡和电气检查、同板 NVS 保存及重载、每个选定条件至少
200 次完整 HELLO 发射，以及独立的受控丢包和恢复试验。与 v3 结果分开记录。
原 M2 台架条件见[测试计划](TEST_PLAN.md)。

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
