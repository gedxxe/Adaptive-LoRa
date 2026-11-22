# 烧录与现场测试

[首页](../../README.zh-CN.md) · [协议](SCHEDULED_LINK.md) · [结果](PROGRESS.md) · [English](../en/BENCH_RUNBOOK.md)

## 上电前

使用 Heltec WiFi LoRa 32 V3.4、合适的 50 欧姆天线或负载，以及输入能力至少 500 mA
的稳定 5 V 电源。即使只操作一块板，也要在上电前接好天线或负载。已保存角色的板卡可能
在启动时发射。核对射频 SKU、接头和天线标记，并在获准的射频条件下做辐射测试。

用 USB-C **数据线**直连一块板。先在 Windows 设备管理器和 `pio device list` 核对
进出时对应的 CP210x 串口。检查端口、USB 设备和其他设备，并记录别名、VID/PID 和 COM。
桥接芯片已枚举但无串口时，安装 [Silicon Labs 驱动](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers)。
端口未确认前不要上传或采集。

## 备份需保留的原固件和配置

打开串口监视器前，连接电源时按住 PRG/BOOT，待启动引脚采样后松开，即进入 ROM 下载模式。
已上电时按住 PRG/BOOT，再短按 RESET。核对实板按钮名称并保持负载连接。使用实际端口、
esptool 5.x 及 Git 工作区之外的私有目录。

```powershell
$python = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$esptool = Join-Path $env:USERPROFILE '.platformio/packages/tool-esptoolpy/esptool.py'
$port = 'COMx'
$dir = 'PRIVATE_DIRECTORY_FOR_THIS_BOARD'
& $python $esptool --chip esp32s3 --port $port --before no-reset --after no-reset --no-stub get-security-info
& $python $esptool --chip esp32s3 --port $port --before no-reset --after no-reset --no-stub flash-id
```

只有安全状态允许读取且闪存确认为 8 MB 时才继续。工具报错或分区异常时停止。不要强制、
擦除或修改 eFuse。用不同文件名独立读取两次。

```powershell
& $python $esptool --chip esp32s3 --port $port --before no-reset --after no-reset read-flash 0x0 0x800000 (Join-Path $dir 'flash-a.bin')
& $python $esptool --chip esp32s3 --port $port --before no-reset --after no-reset read-flash 0x0 0x800000 (Join-Path $dir 'flash-b.bin')
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $dir 'flash-a.bin'), (Join-Path $dir 'flash-b.bin')
```

要求两个文件均为 8,388,608 字节，且哈希一致。对照实际分区、NVS 与待上传镜像的写入
范围。ESP32 SDK 可能在应用启动前格式化 NVS。保留同板私有恢复计划。
见[错误记录 ERR-023](../en/ERROR_LOG.md)。

## 烧录并设置角色

关闭已打开的串口监视器，将 `COMx` 替换为实测端口。各参与板使用同一个自适应镜像。

```powershell
pio device list
$port = 'COMx'
pio run -e heltec_wifi_lora_32_V3_adaptive -t upload --upload-port $port
pio device monitor --port $port --baud 115200
```

**Gateway**

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
```

**首个 Client**

```text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
```

后续 Client 使用不同的节点 ID 3–9。用 `config show` 核对设置，看到 `CONFIG_SAVED`
后发送 `reboot`，再执行 `status`。核对角色、ID、`protocol=4` 和 `profile=adaptive`。
串口配置中的间隔控制示例应用生成数据，空口周期取自 Beacon。

## 双板检查与走测

1. 两块板近距运行几个完整帧。核对同一帧的 Gateway `APP_PAYLOAD` 和 Client
   `V3_PAYLOAD_RESULT,result=accepted`。查看 `ACKED DATA SF`。另做一次 Gateway
   断电上电，确认恢复。
2. 增加固定 Client 3–5。Gateway 及这些节点保持原位，只移动 Client 2。记录出发和停留
   时间、天线朝向和障碍物。查看该节点的 `ADR_APPLY` 变化和之后的 `locked=1`。
3. 按节点记录首传及最终交付、SF、错过时隙、复位和丢日志。`SEARCH` 表示正在监听
   Beacon。屏幕 `ACKED DATA SF` 来自上次确认的 DATA。试验前后运行 `status` 保存
   `ADR_TIMING`。

## 保存结果

采集前关闭监视器。用实测端口，每轮使用新目录。仅采 Gateway 时可用 `--board COMx`。
打开串口可能复位板卡，匹配记录前核对启动边界。

```powershell
$python = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$run = Join-Path $env:LOCALAPPDATA 'Adaptive-LoRa/captures/RUN-YYYYMMDD-HHMMSS'
Get-FileHash .pio/build/heltec_wifi_lora_32_V3_adaptive/firmware.bin
& $python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out $run
```

使用[试验记录](../../logs/test/RUN_RECORD_TEMPLATE.md)保存镜像哈希、板卡别名、时隙和
原始采集。原始日志、设备标识与私有载荷保留在 Git 之外。只有 Gateway 日志时，将 Client 的
ACK 结果标为未知。用高阻探头观察 DIO1、NSS、BUSY 和 SCL，并测量时钟漂移。
对照试验保持路线、负载和无线设置一致。
