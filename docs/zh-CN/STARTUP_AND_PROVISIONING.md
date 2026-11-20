# 启动与配置

同一轮测试中，Gateway 和全部 Client 使用同一构建环境。在装有 pioarduino 或
PlatformIO IDE 扩展的 VS Code 中打开包含 `platformio.ini` 的目录。

| 环境 | `status` 输出 | 用途 |
|---|---|---|
| `heltec_wifi_lora_32_V3` | `protocol=1`、`profile=m2` | HELLO/ACK |
| `heltec_wifi_lora_32_V3_oled` | `protocol=1`、`profile=m2` | 带 OLED 的 HELLO/ACK |
| `heltec_wifi_lora_32_V3_research` | `protocol=2`、`profile=scheduled` | Iteration 019 单 Client 调度 |
| `heltec_wifi_lora_32_V3_multiclient` | `protocol=3`、`profile=multiclient` | Iteration 020 多 Client 载荷调度 |

## 识别板卡并上传

1. 上电前接合适的 50 欧姆负载或天线。已保存有效角色的设备可能在启动时发射。
   首次上传前按 [台架流程](BENCH_RUNBOOK.md) 核对射频条件，并为同一块板保存私有的
   固件和 NVS 备份。
2. 一次只接一块板，使用直连 USB-C 数据线。打开设备管理器的 **端口（COM 和 LPT）**，
   记录 CP210x 的 COM 号，也可运行下面的 PowerShell 命令。关闭占用该端口的监视器。
   若没有 CP210x，先按台架流程检查数据线和驱动。
3. 在 **PlatformIO → Project Tasks** 展开正确环境，在 **General** 中运行 **Build** 和
   **Upload**。使用标准分区及普通上传任务保留 NVS，避免使用 **Erase Flash**。改变分区
   或恢复备份前，先核对地址与安全状态。上传后已有 NVS 周期继续生效。
4. 对同一环境和 COM 口启动 **Monitor**，波特率 **115200**，换行使用 LF 或 CRLF。
   也可运行 `& $pioPython -m platformio device monitor --port COMx --baud 115200`，将
   `COMx` 换成实际端口号。

```powershell
$pioPython = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
& $pioPython -m platformio device list
```

另见 [PlatformIO 的 VS Code 任务说明](https://docs.platformio.org/en/stable/integration/ide/vscode.html#project-tasks)。

## 配置 v3 网络

出厂 Gateway 按 2–9 号顺序接纳 Client，每个应用载荷上限 64 字节。每块 Client
设置唯一 ID。如需改变名单或载荷上限，编辑 `include/research_roster.h` 后重建 v3
镜像。两块板测试时，可仅启用 Client 2，其余节点缺席。串口没有修改名单的命令。
NVS 的 Node ID 标识设备，实际发送须收到 Gateway 的 Beacon 及当帧授权。

Gateway 串口输入：

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
reboot
status
```

Client 1 串口输入：

```text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
reboot
status
```

其余 Client 分别在自己的 COM 口操作，并将 `node-id` 换为名单中分配的 ID。网络和
Gateway ID 均为 1。`config show` 显示暂存值，`CONFIG_SAVED` 表示保存及读回成功，
重启后 `status` 显示生效值和 `protocol=3`。启动时若出现 `CONFIG_INVALID`，设备进入
配置模式并禁止发射。保存时 `CONFIG_ERROR` 给出失败原因。Gateway 如报告
`invalid_schedule_roster`，请核对 ID、名单与周期。`SCHEDULE_CONFIG` 显示名单人数和周期。

示例程序按配置周期尝试提交八字节样例，队列满时跳过此次提交。Gateway 输出
`APP_PAYLOAD`，Client 输出 `V3_PAYLOAD_RESULT`。发射须等待当帧授权，设置 3 秒
并不能保证每 3 秒一次空口发射。详见 [定时链路规范](SCHEDULED_LINK.md)。
