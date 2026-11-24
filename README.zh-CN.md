# Adaptive-LoRa

[English](README.md)

**研究题目**：面向单射频 LoRa 工业能源监测网络、考虑可靠性的 TDMA 调度与链路自适应。

**研究主题与目标。** 研究单台半双工网关怎样为链路条件不同的低速遥测节点分配空口时间，并测量节点增加时的交付率、服务间隔和空口占用。

<img width="570" src="docs/research/figures/project-profile.svg" alt="Heltec V3.4、SX1262、C++17、Python 和 RadioLib 7.8.1">

<a name="烧录与配置"></a>

[烧录与配置](docs/zh-CN/BENCH_RUNBOOK.md) · [协议和公式](docs/zh-CN/SCHEDULED_LINK.md) · [结果与研究计划](docs/zh-CN/PROGRESS.md)

![网关和客户端共用固定控制参数，DATA 的扩频因子按客户端分配](docs/research/figures/adaptive-architecture.svg)

## 工作流程

网关在 SF9 广播信标。客户端申请有效载荷所需的时隙，收到匹配的授权后才发送。网关为每个客户端分配 DATA SF7–SF10，预留重试时隙，并通过 ACK 位图反馈已接收的报文。同一射频在控制阶段和 DATA 阶段之间切换。带宽、编码率和请求发射功率固定。

![一台网关按空口预算轮换分组，每帧服务一组](docs/research/figures/rotation.svg)

默认配置最多服务八个客户端。可选轮换配置最多登记 128 个 ID，每个 15 秒帧选择一组。DATA 参数越慢，每组能容纳的节点可能越少。参见[服务间隔和认证开销模型](docs/zh-CN/SCHEDULED_LINK.md#rotating-schedule)。报文认证和规模化射频测试仍是发布条件。

## 测试方法

所有板卡烧录同一固件，每个客户端使用不同的 ID。先在串口确认协议版本。示例发送 32 字节样本，在网关查找 `APP_PAYLOAD`，并检查接收字节数、DATA SF、ACK 与重试记录。[设置指南](docs/zh-CN/BENCH_RUNBOOK.md#flash-and-set-roles)包含两种角色的命令。

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

其中 $\lambda_\iota$ 为有效载荷大小，$\eta$ 为帧开销，$\sigma_\iota$ 为 DATA SF，$\delta$ 为保护时间。重试池按最长的合格时隙预留空间。完整假设与公式见[空口时间推导](docs/zh-CN/SCHEDULED_LINK.md#airtime-and-slot-budget)和[卡尔曼估计器](docs/zh-CN/SCHEDULED_LINK.md#loss-cause-aware-adr)。

<details>
<summary>网关与客户端伪代码</summary>

<img src="docs/research/figures/adaptive-algorithm-gateway.png" alt="网关服务单个活动组的伪代码" width="480">
<img src="docs/research/figures/adaptive-algorithm-client.png" alt="客户端匹配授权和 ACK 位图的伪代码" width="480">

[LaTeX 源文件](docs/research/figures/adaptive-algorithm.tex)

</details>

## 常见问题

**这个项目用来做什么？** 在单个 LoRa 射频上为低速遥测安排传输时隙。研究面向能源监测，协议承载通用字节数据。

**怎样借助 AI 接入其他系统？** 使用[集成提示词](docs/zh-CN/SCHEDULED_LINK.md#integration-prompt)，先明确载荷、确认语义、时序和测试，再编写适配层。

项目原创代码采用 [GPL-3.0-only](LICENSE)。参见[第三方条款和发布义务](THIRD_PARTY.md)。
