# Adaptive-LoRa

[简体中文](README.zh-CN.md)

One Heltec Gateway uses a single SX1262 to schedule up to eight telemetry
Clients. The Gateway assigns a DATA spreading factor (SF) to each Client
and groups acknowledgements in a bitmap so only missing packets can retry.

<a name="flash-and-configure"></a>

**[Flash, set roles and run a walk test](docs/en/BENCH_RUNBOOK.md)** ·
[Protocol and equations](docs/en/SCHEDULED_LINK.md) ·
[Results and research references](docs/en/PROGRESS.md)

![Adaptive-LoRa architecture with common control and per-Client DATA profiles](docs/research/figures/adaptive-architecture.svg)

## How it works

The same image runs on every board. A Beacon starts the frame. The Grant
assigns a slot, byte count and DATA SF. Control packets share one SF, so
Clients can recover synchronization after missing a frame.

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

## Airtime budget

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

Here $\lambda_\iota$ is the application payload, $\eta$ adds the frame
overhead, $\sigma_\iota$ is the selected SF, and $\delta$ is the guard time.
The retry term covers $\varrho$ rounds of the longest eligible slots.
The [derivation](docs/en/SCHEDULED_LINK.md#airtime-and-slot-budget) defines
each symbol, expands the control overhead and lists the settings used
by the current firmware.

Host tests cover mixed SFs, missing control packets and recovery. Radio
timing and range still need measurements on the boards.
[What has been tested](docs/en/PROGRESS.md)
