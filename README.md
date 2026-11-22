# Adaptive-LoRa

[简体中文](README.zh-CN.md)

Adaptive-LoRa is a research prototype for scheduled telemetry on one
half-duplex SX1262. The current test profile serves up to eight Clients.
The Gateway grants a DATA spreading factor (SF) per Client and limits
each new SF to the airtime available to the full roster.

**Eight-Client test.** Build the default `heltec_wifi_lora_32_V3_adaptive`
image for all nine boards. Verify wire version 5 after flashing. Use Gateway
ID 1 and Client IDs 2–9. The 15 s frame reserves up to
16 payload bytes per Client. The example sends 8 bytes.

During a walk, read the top line on any page. `TX SF8 CR4/5` on the Client
identifies its last DATA attempt. The Gateway shows the last received DATA SF.
The Gateway adjusts DATA SF7–SF10 per Client, sizes slots to the requested
bytes and grants retries from the ACK bitmap. Control stays at SF9. Coding
rate, bandwidth, TX power and frame period stay fixed.

<a name="flash-and-configure"></a>

**[Flash, set roles and run the eight-Client test](docs/en/BENCH_RUNBOOK.md)** ·
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

The model reserves 14.084 s when all eight Clients use SF9 at the test
ceiling, including the 300 ms end reserve. This leaves 0.916 s of additional
headroom. Coverage and timing still need measurements on the boards.
With the other Clients at SF7, up to two can reserve SF10. The Gateway may
grant fewer if other Clients already use a higher SF. [Capacity and SF10](docs/en/SCHEDULED_LINK.md#sf10-capacity-and-higher-sf-options) ·
[Results and the 70-Client research plan](docs/en/PROGRESS.md)
