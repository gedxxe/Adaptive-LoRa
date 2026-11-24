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
后发送 `reboot`，再执行 `status`。在每块板上核对角色、ID、`protocol=5` 和 `profile=adaptive`。
试验前同时升级 Gateway 和 Client 的旧自适应镜像。旧版 Gateway 若仍广播 16 B 上限，
新版 Client 不会发送 Request。串口配置中的间隔控制 Client 提交样本的时间，空口周期取自 Beacon。
Gateway 的 `SCHEDULE_CONFIG` 应显示 `period_us=15000000`、`roster_count=8`
和 `bench_payload_ceiling_bytes=32`。
走测前核对 `data_sf_policy=adaptive`。Gateway 的链路样本应显示 `params_version=4`。

Client 2 的首个样本为 `ID=0002|SEQ=0001|MSG=HELLO WORLD`。ID 和序号均为四位十六进制。
启动后序号从 1 开始，每次有新样本入队时递增，到 `FFFF` 后回绕。DATA 帧携带恰好 32 字节
ASCII，不带字符串结束符或换行符。重传沿用相同字节和包标识。

## 双板检查与走测

1. Client 2 与 Gateway 近距运行至少五个完整帧。核对同一帧的 Gateway `APP_PAYLOAD` 和 Client
   `V3_PAYLOAD_RESULT,result=accepted`。检查 Gateway 的 `bytes=32` 和十六进制载荷，再查看
   Client 的 `ACKED DATA SF`。另做一次 Gateway
   断电上电，确认恢复。
2. 增加固定 Client 3–5 并采集至少五个完整帧，再加入 Client 6–9，全部固定运行至少二十个
   完整帧。确认各节点都有 DATA 交付和 ACK，且没有应用重复交付、意外复位、串口数据缺失
   或无法解释的时序错误。
3. 另开一轮，仅移动 Client 2，其他节点和 Gateway 保持原位。记录出发和停留时间、天线
   朝向和障碍物。SF 变化后至少等待六个完整帧，查看锁定或回退，并对照其余节点交付情况。
4. 按节点记录首传及最终交付、SF、错过时隙、复位和丢日志。`SEARCH` 表示正在监听
   Beacon。屏幕 `ACKED DATA SF` 来自上次确认的 DATA。试验前后运行 `status` 保存
   `ADR_TIMING`。

单独检查 SF10 时，先看 Client 是否显示 `TX SF10`，再核对 `ACKED DATA SF10` 和
SF9 控制交互是否继续。若未获得 SF10，核对是预算不足（`adr=budget_limited`），还是
控制链路丢失（`SEARCH` 或 Request/Grant 缺失）。这一检查与静态验收分开进行。

**单人走测。** Client 各页顶行的 `TX SF8 CR4/5` 表示最近一次 DATA 已在 SF8
启动发送。切回 SF9 收控制帧后仍保留该读数。第二页的 `ACKED DATA SF` 只在收到匹配
ACK 后更新。到达测点后记下这两个值及第一页的 AGE。Client 的 AGE 从最近一次有效
Grant 或位图算起，断联后仍会保留上一次的 TX 值。`SEARCH` 表示正在等待新 Beacon。
`NO GRANT` 表示 Request 已启动发送，但未收到匹配 Grant。`REQ FAIL` 表示本地请求
发送失败，`NO TX` 表示已授权但未启动 DATA。第二页的 `APP MISSING` 表示反馈未确认本次
DATA，`APP UNKNOWN` 表示无法确认是否交付。
Gateway 顶行显示最近一次有效 DATA 的接收 SF，第一页显示发送者。短横线表示本会话尚无
DATA 事件。屏幕在无线收发阶段之间刷新。

走测出现断联时，原地等待六个帧并记录 Client 屏幕。返回 SF9 覆盖范围后检查第一次新 DATA。
启动会话未变且仍有足够的空口预算时，Gateway 可以恢复上次确认的 SF，或试用最近一次
收到 DATA 时使用的更高 SF。让一个固定节点继续记录。若要检查控制仍通而 DATA 丢失的
恢复过程，在同一位置最多等待十个帧，给 Gateway 留出识别丢失、试用更高 SF 和收取回执的时间。
核对移动节点的 `TX SF`、`ACKED DATA SF` 以及固定节点的交付情况。

**同条件 SF7 对照。** 沿同一路线复测，只需更换 Gateway 镜像。将 `COMx` 换为实测
端口。以下命令在子进程中构建，输出到独立目录：

```powershell
cmd /d /c 'set PLATFORMIO_BUILD_SRC_FLAGS=-DADAPTIVE_LORA_FIXED_SF_COMPARISON&&set PLATFORMIO_BUILD_DIR=.pio/comparison-build&&pio run -e heltec_wifi_lora_32_V3_adaptive -t upload --upload-port COMx'
```

运行 Gateway 的 `status`，记录 `data_sf_policy=fixed_sf7`。Client 镜像、SF9 控制交互、
15 秒周期及重传规则与自适应试验相同。恢复时重新烧录普通自适应镜像，确认
`data_sf_policy=adaptive`。

**实验 ADR 对照。** 下列命令构建并烧录 EWMA 版 Gateway。将参数改为 `=2`，并使用 `.pio/adr-kalman`，
即可构建 Kalman 版。将 `COMx` 换成实际端口，核对 `adr_rule`，沿用相同 Client、路线与负载。
规则在构建时选择，无需修改配置。

```powershell
cmd /d /c 'set PLATFORMIO_BUILD_SRC_FLAGS=-DADAPTIVE_LORA_ADR_RULE=1&&set PLATFORMIO_BUILD_DIR=.pio/adr-ewma&&pio run -e heltec_wifi_lora_32_V3_adaptive -t upload --upload-port COMx'
```

## 保存结果

采集前关闭监视器。用实测端口，每轮使用新目录。按节点 ID 顺序重复传入 `--client`。
仅采 Gateway 时可用 `--board COMx`。
打开串口可能复位板卡，匹配记录前核对启动边界。

```powershell
$python = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$run = Join-Path $env:LOCALAPPDATA 'Adaptive-LoRa/captures/RUN-YYYYMMDD-HHMMSS'
Get-FileHash .pio/build/heltec_wifi_lora_32_V3_adaptive/firmware.bin
& $python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out $run
```

八个 Client 时使用新的 `$run`，并填入 ID 2–9 的实际端口：

```powershell
$ports = @('--gateway', 'COM_GATEWAY')
$clients = @('COM_ID2', 'COM_ID3', 'COM_ID4', 'COM_ID5', 'COM_ID6', 'COM_ID7', 'COM_ID8', 'COM_ID9')
foreach ($client in $clients) { $ports += @('--client', $client) }
& $python tools/capture_bench.py @ports --seconds 360 --out $run
```

使用[试验记录](../../logs/test/RUN_RECORD_TEMPLATE.md)保存镜像哈希、板卡别名、时隙和
原始采集。原始日志、设备标识与私有载荷保留在 Git 之外。只有 Gateway 日志时，将 Client 的
ACK 结果标为未知。

每个完整帧、每个 Client 记录一行 CSV。建议字段如下。

```text
run_id,gateway_boot,frame_id,node_id,client_boot,grant_seen,payload_bytes,data_sf,first_tx,first_received,app_accepted,client_ack,attempts,rssi_dbm,snr_db,rx_us,timing,segment
```

仅在同一 Gateway 启动会话内匹配 `V3_LINK_SAMPLE`、`APP_PAYLOAD` 与 Client 侧
`V3_DATA_TX`、`V3_PAYLOAD_RESULT`。遇到复位或人为中断时另开一段。去掉首尾不完整帧，
缺失或无法匹配的结果标为未知。`class=ABSENT` 表示 Gateway 未收到有效 Request。
仅凭缺少日志行无法判断发生了什么。
`timing=invalid` 属于本地调度异常，从射频对比中排除，已记录的应用交付仍计入端到端结果。
`rx_us` 为接收时的 Gateway 启动后时间，可用于统计同一会话内单个 Client 的交付间隔。
若出现 `adr=rx_window_unobserved`，Gateway 既未在发送前确认已用指定 SF 监听，也未在该时隙
收到有效 DATA。`class=PRIMARY_MISS` 表示首传有可用的接收窗口却未收到 DATA，
即便帧内其他时序存在异常。Gateway 可用该记录启动限时 SF 试用。
`adr=loss_unconfirmed` 表示 Gateway 未收到 DATA，且本次
Request 未显示余量偏低。按会话、帧编号和节点匹配 Client 的 `V3_DATA_TX` 与 Gateway 的样本，
再判断丢失原因。`recovery_probe` 和 `recovery_next` 表示申请试用 SF，应与相应
`V3_DATA_TX` 核对。`recovery_confirmed` 表示回执匹配，`recovery_budget` 表示预算不足，
`ADR_PROBE` 的 `result=expired` 表示试用未确认且已到期限。

用高阻探头观察 DIO1、NSS、BUSY 和 SCL。每种测试使用的 SF 切换方向至少记录 30 次，包含
回到控制 SF 的切换，核对延迟尾部和最大值能否满足 20 毫秒切换提前量及 40 毫秒接收半窗。
在完整 15 秒帧内测量漂移。先查清错过时隙和意外复位，再用走测比较射频表现。
