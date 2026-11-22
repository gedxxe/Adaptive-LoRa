# Results and research

[Home](../../README.md) · [Field test](BENCH_RUNBOOK.md) · [Protocol](SCHEDULED_LINK.md) · [简体中文](../zh-CN/PROGRESS.md)

<!-- current-state:start -->
| Check | Result |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `5` |
| Board | Heltec WiFi LoRa 32 V3.4, ESP32-S3/SX1262 |
| Adaptive image | `heltec_wifi_lora_32_V3_adaptive`, ESP32 build passed, 17/17 native cases |
| Fixed-SF image | `heltec_wifi_lora_32_V3_multiclient`, ESP32 build passed, 15/15 runtime cases |
| Shared tests | 356/356 native cases and 61/61 Python tests in Iteration 026 |
| Field test | Walk test and radio timing measurements pending |
<!-- current-state:end -->

| Claim | Verification |
|---|---|
| Eight Clients can all obtain SF9 at the 16 B bench ceiling | Production state machines over fake radio. Real delivery and link range still need measurement. |
| The 15 s frame has 0.916064 s beyond its end reserve at that load | Analytical model checked against the production calendar. |
| Two SF10 Clients can deliver alongside six SF7 peers | Fake-radio tests cover SF9 control, an SF10 retry, a refused third SF10 upgrade, and reuse of the reservation after a successful drop to SF9. |
| A Client waits on SF9 when control is lost | Fake-radio outage after SF10 delivery. The next valid Beacon and Grant restored delivery. |
| A local final ACK timing fault preserves the recorded delivery without training ADR | Native fault injection. |
| A Gateway stall across one Client's primary RX slot leaves the other Client's ADR active | Fake radio and delayed Gateway loop. Both Clients delivered, one by retry. |
| The capture tool records nine streams with separate timestamps and hashes | Simulated serial ports. |

[Capacity matrix](SCHEDULED_LINK.md#capacity-at-15-seconds) · [Build and test records](BUILD_MEMORY.md)

## Papers and comparison runs

[Research-to-code map](../research/ALGORITHM_TRACEABILITY.md) ·
[Paper references and figure sources](../references/README.md)

| Use | PlatformIO environment | Wire version |
|---|---|---:|
| Adaptive field test and PlatformIO default | `heltec_wifi_lora_32_V3_adaptive` | 5 |
| Earlier fixed-SF comparison | `heltec_wifi_lora_32_V3_multiclient` | 3 |
| Single-Client TDMA comparison | `heltec_wifi_lora_32_V3_research` | 2 |
| HELLO/ACK comparison | `heltec_wifi_lora_32_V3` | 1 |

Use the same wire version on every board in a run. The [matched SF7 Gateway](BENCH_RUNBOOK.md#check-the-pair-and-walk-the-route)
keeps the adaptive wire format, control PHY, period and retry policy. Keep
antenna placement, offered load and route fixed between runs. The older
fixed-SF image also changes the control PHY and frame timing.
`PROTOCOL_VERSION` tracks the HELLO/ACK image. The other wire versions
are selected by the build. All images share `VERSION` and NVS schema 1.

## Research target for 70 Clients

Test when a single half-duplex radio can keep low-rate telemetry fresh.
The target is 70 Clients per Gateway with a 2–5 minute reporting interval.
The running firmware serves eight active roster positions. A first model
can partition 70 IDs into nine rosters, taking 135 s for one scheduled
opportunity per Client at the current 15 s frame period. That number
describes the schedule alone. Loss, retry contention, outage, and sample
generation can all increase the gap between fresh readings.

```math
\tau_{\mathrm{cycle}}=\left\lceil\frac{\nu}{\nu_{\mathrm{group}}}\right\rceil\tau_{\mathrm{frame}},
\qquad
\Delta_{\iota,\jmath}=\tau^{\mathrm{rx}}_{\iota,\jmath}-\tau^{\mathrm{rx}}_{\iota,\jmath-1}.
```

$\nu$ is the population, $\nu_{\mathrm{group}}$ the active group limit,
and $\Delta_{\iota,\jmath}$ the gap between accepted readings for Client
$\iota$. A candidate also has to meet the allowed Gateway transmit airtime
over the applicable interval. Rotating unchanged frames spreads access
across Clients while the Gateway still pays control airtime per group.

1. Measure timing, first-pass and final delivery, retry count, and the
   per-Client gap between accepted readings on 1, 4, then 8 boards plus
   the Gateway. Use multiple independent runs and report the worst node
   as well as run-level intervals. Add synchronized sample timestamps
   before reporting age of information.
2. Model group rotation with per-Client SF and payload limits. Test
   off-roster waiting, state preservation across groups, fairness, and
   recovery before changing the protocol. Validate 70 logical identities
   separately from 70 independent RF links. Evaluate SF11–SF12 with LDRO
   only with revised airtime and measured transition budgets.
3. Compare matched single-radio fixed-SF TDMA and contention baselines on
   the same Heltec hardware. Isolate the admission policy, retry pool and
   the cost of per-frame Request/Grant against persistent slots. Treat
   concentrator results as simulations unless a physical comparator is run.

Before an unattended pilot, design authenticated frames and replay
protection, watchdog recovery with fault injection, and application-level
deduplication and buffered telemetry. CRC and boot/frame IDs do not
authenticate traffic. A single radio and one channel leave common-mode
outage and external interference. Antenna orientation and placement, loss
across shifts, SF probe thresholds and the delivery cost of the 28 B header
need field data. The current profile is for controlled
testing under an authorized RF setup. Set publication claims after the
measurements and match venue scope and disclosure rules at submission.

## Past work

[Earlier manuals, hardware checks and test plans](https://github.com/gedxxe/Adaptive-LoRa/tree/4ab3655dc8ca3b7f9ddc382d8ebeeeadceb6d2c7/docs).
This pinned Git snapshot keeps the documents from before consolidation.
The project also keeps its [build log](BUILD_MEMORY.md),
[decisions](DECISIONS.md) and [error log](ERROR_LOG.md). The
[log index](../../logs/README.md) explains which files are historical.
