# Adaptive-LoRa

[English](README.md)

Adaptive-LoRa 是使用单颗半双工 SX1262 的遥测调度原型。当前测试配置最多支持八个 Client。
Gateway 为各 Client 指定 DATA 扩频因子（SF），并按整组节点的空口时间预算限制 SF 调整。

**八节点测试。** 九块板都烧录默认镜像 `heltec_wifi_lora_32_V3_adaptive`，并确认空口版本为 5。Gateway ID 为 1，
Client ID 为 2–9。每个 Client 提交 32 字节 ASCII 样本，例如
`ID=0002|SEQ=0001|MSG=HELLO WORLD`，15 秒超帧按这个长度预留时隙。
在 Gateway 的 `APP_PAYLOAD` 日志中检查 `bytes=32`。

走测时可直接查看任一页顶行。Client 显示的 `TX SF8 CR4/5` 表示最近一次 DATA 发送尝试，
Gateway 显示最近一次收到的 DATA 的 SF。Gateway 为各 Client 调整 DATA SF7–SF10，按申请的字节数
分配时隙，并通过 ACK 位图分配重传机会。控制帧固定用 SF9，编码率、带宽、发射功率和帧周期均固定。

Gateway 分别跟踪各 Client 的交付。最近三条有效记录中若有两次丢失，可在空口预算允许时
试用更高 DATA SF。如果首传接收窗已观测到，后续 ACK 或重传时序异常仍可保留该次丢失记录。
试用结果以 ACK 回执确认。同一会话断联后重入，只要预算允许，即可恢复已确认的 SF。
`SEARCH` 表示在等新的 SF9 Beacon。
`NO GRANT` 表示已启动 Request，但未收到匹配的 Grant。
Gateway 还提供两种[基于 SNR 余量的实验规则](docs/zh-CN/SCHEDULED_LINK.md#按丢包情况调整)。

<a name="烧录与配置"></a>

**[烧录、设置角色并进行八节点测试](docs/zh-CN/BENCH_RUNBOOK.md)** ·
[协议与公式](docs/zh-CN/SCHEDULED_LINK.md) ·
[测试结果与研究参考](docs/zh-CN/PROGRESS.md)

![Adaptive-LoRa 架构：统一控制 SF、各 Client 独立 DATA SF](docs/research/figures/adaptive-architecture.svg)

## 工作流程

控制帧使用 $\sigma_{\mathrm{ctrl}}$，Client $\iota$ 使用 $\sigma_\iota$ 发送 DATA。
Gateway 先为整组节点 $\Omega$ 预留时间，再用 $\Xi$ 表示已接收的 DATA，
用 $\Pi_\jmath$ 表示第 $\jmath$ 轮宣布的重传时隙。重传沿用同一个包 $\Delta_\iota$。

<img src="docs/research/figures/adaptive-algorithm-gateway.png" alt="Gateway 算法。恢复各节点的 SF、核算空口预算、收发 DATA 与 ACK，并用有效记录评估链路。" width="480">

<img src="docs/research/figures/adaptive-algorithm-client.png" alt="Client 算法。等待 SF9 控制帧，在匹配的 Grant 下按指定 SF 发送 DATA，区分未确认和结果未知。" width="480">

[SF 恢复和评估伪代码](docs/zh-CN/SCHEDULED_LINK.md#自适应与恢复) ·
[LaTeX 源文件](docs/research/figures/adaptive-algorithm.tex)

## 空口时间预算

```math
\begin{aligned}
\tau_{\mathrm{slot},\iota}
  &= \tau_{\mathrm{air}}(\lambda_\iota+\eta,\sigma_\iota)+\delta,\\
\tau_{\mathrm{used}}
  &= \tau_{\mathrm{ctrl}}+\tau_{\mathrm{data}}
   +\varrho\,\tau_{\mathrm{retry}}+\tau_{\mathrm{tail}},\\
\tau_{\mathrm{used}} &\leq \tau_{\mathrm{frame}}.
\end{aligned}
```

$\lambda_\iota$ 表示应用载荷，$\eta$ 表示帧开销，$\sigma_\iota$ 为选定 SF，
$\delta$ 为保护间隔。重传项为 $\varrho$ 轮中耗时最长的可用时隙预留时间。
[推导过程](docs/zh-CN/SCHEDULED_LINK.md#空口时间和时隙预算)解释各符号、展开控制开销，
并列出现有固件参数。

按每个样本 32 字节计算，其他 Client 均用 SF7 时，15 秒内最多可安排五个 SF9
或两个 SF10 节点。两个 SF10 的组合在帧尾预留之外还剩 77 毫秒。
覆盖距离和无线时序仍需实测。
[容量与 SF10](docs/zh-CN/SCHEDULED_LINK.md#sf10-容量及更高-sf-选项) · [实验结果与 70 节点研究计划](docs/zh-CN/PROGRESS.md)
