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

所有板使用同一个镜像。Beacon 启动一帧，Grant 指定时隙、字节数和 DATA SF。
控制帧采用同一个 SF，Client 丢帧后可在该 SF 上恢复同步。

```text
gateway(frame):
    requests <- beacon_and_collect()
    plan <- fit_grants(requests)
    if not plan: return
    broadcast_grant(plan)
    for round in plan:
        receive_data_at_granted_sfs(round)
        bitmap <- send_ack_at_control_sf()
        if tx_done(bitmap): open_retries()
    update_sf(delivery, snr, ack_receipts)

client(frame):
    listen_on_control_sf()
    if no fresh Beacon or no DATA: return
    request_slot(frame, last_ack)
    grant <- matching_grant(frame)
    if not grant: return
    send_in_slot(grant)
    for ack in valid_bitmaps_on_control_sf():
        if accepted(ack): return save_ack(ack)
        if retry_granted(ack): retry_in_slot(ack)
```

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
