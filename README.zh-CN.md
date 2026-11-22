# Adaptive-LoRa

[English](README.md)

Adaptive-LoRa 是面向工业遥测的 LoRa 网络。一台 Gateway 用一颗 SX1262 服务最多八个
Client，为每个 Client 分配发射时隙，并可单独选择其数据扩频因子（SF）。各板使用同一固件，
启动时从已保存的配置读取角色。

![单射频 Gateway 与最多八个 Client 的架构，SF9 用于控制，各 Client 在独立时隙发数据](docs/research/figures/adaptive-architecture.svg)

## 工作流程

Gateway 首先广播 Beacon。有数据待发的 Client 申请时隙，Gateway 通过 Grant 指定
载荷长度和 DATA SF。Client 依次发送后，Gateway 用一份 ACK 位图公布接收结果和
重传许可。Beacon、Request、Grant 和 ACK 均使用 SF9。DATA 及其重传使用授权的 SF7–SF9。

![Beacon、时隙请求、Grant、DATA、ACK 位图和选择性重传的顺序](docs/research/figures/adaptive-frame.svg)

某个 Client 的首轮传输反复失败时，Gateway 会在时序允许的情况下提高 SF。交付情况和
信号质量改善后，可试用较低 SF，失败时回到上一个设置。连续三次首轮交付成功且收到该
Client 的匹配 ACK 回执后，Gateway 才确认该 SF 稳定。

```math
\begin{aligned}
&\textbf{Gateway per superframe}\\
&\quad\text{Send Beacon and collect Requests on SF9}\\
&\quad\text{If requested, fit slots and send Grant}\\
&\quad\textbf{For each DATA or retry round}\\
&\qquad\text{Receive DATA at each assigned SF}\\
&\qquad\text{Send ACK bitmap with retry grants on SF9}\\
&\quad\text{Update SF from delivery, SNR and receipts}\\[3pt]
&\textbf{Client}\\
&\quad\text{Request a slot and echo the last ACKed frame}\\
&\quad\text{Send DATA under a matching Grant}\\
&\quad\text{Return to SF9 and check the ACK bitmap}
\end{aligned}
```

### 空口时间预算

设 $b$ 为应用载荷字节数，$s$ 为扩频因子。额外 32 字节用于帧头和 Gateway boot ID。
公式采用 125 kHz 带宽、CR 4/5、8 个前导符号和 PHY CRC。

```math
\begin{aligned}
n(b,s)&=\left\lceil\frac{8(b+32)+44-4s}{4s}\right\rceil\\
T_{\mathrm{slot}}(b,s)&=\left(0.080+
\bigl(20.25+5n(b,s)\bigr)\frac{2^s}{125000}\right)\mathrm{s}.
\end{aligned}
```

Gateway 按完整名单 $R$ 中各节点的载荷上限 $B_i$ 检查新 SF 是否有足够时间。
每轮重传为最长的四个时隙 $H_4$ 预留时间。$T_{\mathrm{ctrl}}$ 包含控制帧和其余保护间隔。

```math
\begin{aligned}
T_{\mathrm{reserved}}={}&T_{\mathrm{ctrl}}+
\sum_{i\in R}T_{\mathrm{slot}}(B_i,s_i)\\
&+3\sum_{i\in H_4}T_{\mathrm{slot}}(B_i,s_i)+0.300\,\mathrm{s}
\ \leq\ 15\ \mathrm{s}.
\end{aligned}
```

按八个 Client 各预留 64 字节、其余节点用 SF7 计算，两个 SF9 链路需预留
14.157 秒，三个需要 15.634 秒。这些数值来自模型计算。

走测镜像采用 **15 秒超帧**、每个 Client **最多 64 字节数据**，无线参数为
**915.0 MHz / 125 kHz / CR 4/5**。主机测试覆盖不同 SF 的通信、控制帧丢失后的恢复、
以及一个移动 Client 与三个固定 Client 的模拟。实板覆盖和时序仍需测量。
[研究参考与测试结果](docs/research/ALGORITHM_TRACEABILITY.md)

## 烧录与配置

使用 **Heltec WiFi LoRa 32 V3.4**。上电前连接合适的 50 欧姆天线或负载，并在获准的
射频条件下测试。需要保留原固件或配置时，先完成同板备份，再打开串口或上传。
[首次上电检查与备份](docs/zh-CN/BENCH_RUNBOOK.md)

用 USB 数据线逐块连接，在 PlatformIO 终端中将 `COMx` 换成该板的实际端口。
上传前关闭已打开的串口监视器。两种角色使用同一个镜像。

```powershell
pio device list
$port = 'COMx' # 换成实际端口
pio run -e heltec_wifi_lora_32_V3_adaptive -t upload --upload-port $port
pio device monitor --port $port --baud 115200
```

在 VS Code 中也可选择 **PlatformIO → Project Tasks →
heltec_wifi_lora_32_V3_adaptive → General → Upload**，再打开 **Monitor**。
若没有 COM 口，按 [USB 检查步骤](docs/zh-CN/BENCH_RUNBOOK.md#windows-设备管理器)排查。

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

后续 Client 使用不同的节点 ID **3–9**。用 `config show` 检查待保存值，
看到 `CONFIG_SAVED` 后发送 `reboot`，必要时重新打开串口，再执行 `status`。
核对角色、ID、`protocol=4` 和 `profile=adaptive`。3000 ms 为示例数据生成间隔，
实际空口发射仍按 Gateway 的 15 秒调度执行。

## 验证链路

先近距测试一个 Gateway 和一个 Client。核对同一帧的 Gateway `APP_PAYLOAD` 和
Client `V3_PAYLOAD_RESULT`，后者应包含 `result=accepted`。首个 SF7 载荷确认后，
Client 的 DATA 页显示 `ACKED DATA SF7`。走测时固定 Client 3–5，只移动 Client 2。
记录其 SF 变化及固定节点是否持续交付。 [走测和日志采集](docs/zh-CN/BENCH_RUNBOOK.md#自适应-v4-走测)

## 参考

- [固件架构与模块职责](docs/zh-CN/ARCHITECTURE.md)
- [无线参数、帧格式与时序](docs/zh-CN/SCHEDULED_LINK.md)
- [软件检查、现场结果与对照镜像](docs/zh-CN/PROGRESS.md)
