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

Control uses $\sigma_{\mathrm{ctrl}}$ and each Client $\iota$ sends DATA at
$\sigma_\iota$. The Gateway fits the full roster $\Omega$ into the frame
budget, then uses $\Xi$ for accepted DATA and $\Pi_\jmath$ for retry slots
announced in round $\jmath$. A retry reuses the same packet $\Delta_\iota$.

<img src="docs/research/figures/adaptive-algorithm-gateway.png" alt="Gateway algorithm. Collect requests, fit the schedule, receive DATA, send the ACK bitmap and review the per-Client SF." width="480">

<img src="docs/research/figures/adaptive-algorithm-client.png" alt="Client algorithm. Synchronize to a Beacon, request a slot, transmit with the granted SF, then accept the ACK or use an assigned retry slot." width="480">

[Algorithm notation and SF decisions](docs/en/SCHEDULED_LINK.md#adaptation-and-recovery) ·
[LaTeX source](docs/research/figures/adaptive-algorithm.tex)

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
