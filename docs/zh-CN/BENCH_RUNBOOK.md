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

## 自适应 v4 走测

上传前按[硬件指南](HARDWARE.md)检查板卡标记、天线、接头、闪存容量和供电。新板需核对
显示接线和 Vext 极性。在获准的射频条件下确认供电及天线或负载能支持请求的 22 dBm。
按 [README](../../README.zh-CN.md#烧录与配置)上传并设置角色。网络 1 内使用 Gateway 1、
移动 Client 2 和固定 Client 3–5。逐板核对镜像及串口 `status`。

```powershell
Get-FileHash -Algorithm SHA256 .pio/build/heltec_wifi_lora_32_V3_adaptive/firmware.bin
```

1. 先近距运行一对板，共四个完整超帧，约一分钟。核对 `APP_PAYLOAD` 和 Client 的
   `V3_PAYLOAD_RESULT,result=accepted`，并查看 Client 是否显示 `ACKED DATA SF7`。
   另做一次 Gateway 断电上电，确认 Client 恢复。
2. 固定 Gateway 和 Client 3–5，连续采集。Client 2 在近处停留一分钟，沿路线行走，并
   在目标点停留约两分钟。记录出发和到达时间，保持天线朝向一致。
3. 逐个 Client 比较首轮交付、重传后交付、SF 变化和中断时长。检查 `deadline_miss`、
   复位和丢日志。试验前后用 `status` 记录 `ADR_TIMING`。事件含义见[串口日志](LOGGING_GUIDE.md)。

## 采集串口日志

采集前关闭监视器，并使用 Git 工作区之外的新目录。下方双板示例的端口应换成实际值。
走测时可对固定 Gateway 使用 `--board COMx`，有第二台电脑时再单独采集移动 Client，
两端使用同一试验编号。打开串口可能导致复位，应核对日志中的启动边界。

```powershell
$pioPython = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$captureDir = Join-Path $env:LOCALAPPDATA 'Adaptive-LoRa/captures/RUN-YYYYMMDD-HHMMSS-smoke'
& $pioPython tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out $captureDir
```

按[试验记录](../../logs/test/RUN_RECORD_TEMPLATE.md)保存镜像哈希、板卡别名、供电、
天线位置和原始日志。如果只有 Gateway 日志，Client 的 ACK 或控制帧丢失应标为未知。
发布前去除设备标识符和私有载荷，保留试验时间与结果。调整保护间隔前，使用高阻探头
观察 DIO1、NSS、BUSY 和 SCL，测量中断处理、恢复接收、屏幕访问及时钟漂移。

固定 SF 对照使用[版本说明](../en/VERSIONING.md)中的镜像，保持板卡、载荷、功率、天线和
路线一致，记录控制 SF 与帧周期的差异。早期 HELLO/ACK 流程和至少 200 个事务的要求见
[测试计划](TEST_PLAN.md)。

<details>
<summary>BUSY 故障注入流程</summary>

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

</details>
