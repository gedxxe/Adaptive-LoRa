# Adaptive-LoRa

[English](README.md)

一台 Heltec Gateway 用一颗 SX1262 调度最多八个遥测 Client。Gateway 为各 Client
分配 DATA 扩频因子（SF），并用 ACK 位图公布收包结果，只有缺失包可以重传。

<a name="烧录与配置"></a>

**[烧录、设置角色并走测](docs/zh-CN/BENCH_RUNBOOK.md)** ·
[协议与公式](docs/zh-CN/SCHEDULED_LINK.md) ·
[测试结果与研究参考](docs/zh-CN/PROGRESS.md)

![Adaptive-LoRa 架构：统一控制 SF、各 Client 独立 DATA SF](docs/research/figures/adaptive-architecture.svg)

## 工作流程

控制帧使用 $\sigma_{\mathrm{ctrl}}$，Client $\iota$ 使用 $\sigma_\iota$ 发送 DATA。
Gateway 先为整组节点 $\Omega$ 预留时间，再用 $\Xi$ 表示已接收的 DATA，
用 $\Pi_\jmath$ 表示第 $\jmath$ 轮宣布的重传时隙。重传沿用同一个包 $\Delta_\iota$。

<img src="docs/research/figures/adaptive-algorithm-gateway.png" alt="Gateway 算法。收集请求、分配时隙、接收 DATA、发送 ACK 位图，并评估各 Client 的 SF。" width="480">

<img src="docs/research/figures/adaptive-algorithm-client.png" alt="Client 算法。根据 Beacon 同步并申请时隙，按分配的 SF 发送 DATA，再处理 ACK 或重传。" width="480">

[算法符号与 SF 调整](docs/zh-CN/SCHEDULED_LINK.md#自适应与恢复) ·
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

主机测试覆盖不同 SF、控制帧丢失及恢复。无线时序与覆盖距离仍需上板测量。
[查看测试结果](docs/zh-CN/PROGRESS.md)
