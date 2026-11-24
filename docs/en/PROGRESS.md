# Results and research

[Home](../../README.md) · [Field test](BENCH_RUNBOOK.md) · [Protocol](SCHEDULED_LINK.md) · [简体中文](../zh-CN/PROGRESS.md)

<!-- current-state:start -->
| Check | Result |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `5` |
| Board | Heltec WiFi LoRa 32 V3.4, ESP32-S3/SX1262 |
| Adaptive image | `heltec_wifi_lora_32_V3_adaptive`, ESP32 builds passed for Legacy, EWMA and Kalman. Native cases 40/40. |
| Fixed-SF image | `heltec_wifi_lora_32_V3_multiclient`, ESP32 build passed, 15/15 runtime cases |
| Shared tests | 356/356 native and 65/65 Python cases passed |
| Toolchain | PlatformIO 6.1.19, Espressif32 55.3.311, Arduino ESP32 3.3.11, Xtensa 14.2.0+20260121, RadioLib 7.8.1 |
| Field test | Walk test and radio timing measurements pending |
<!-- current-state:end -->

The operator reported SF adaptation through SF10 on an earlier one-pair
walk. The current 32 B profile still needs a field run.

| Claim | Verification |
|---|---|
| The adaptive image sends exact 32-byte ASCII samples | Native runtime test decodes two samples and checks identical retry frames. |
| Eight Clients can deliver 32 B at five SF9 and three SF7, or two SF10 and six SF7 | Production state machines over fake radio. The two-SF10 test covers a retry and rejection of a third increase. |
| The two-SF10 mix leaves 77.408 ms beyond the end reserve | Analytical model. Real delivery and link timing still need measurement. |
| A Client waits on SF9 when control is lost | Fake-radio outage after SF10 delivery. The next valid Beacon and Grant restored delivery. |
| A returning Client can reuse its confirmed SF without taking a peer's reservation | Hosts drive the real Gateway and Client state machines through idle expiry, a late ACK receipt and limited SF10 airtime. |
| An unsuccessful downshift cannot leave a Client at an unconfirmed lower SF | Fake-radio tests cover a failed trial, missing ACK receipts and a control outage during a trial. |
| Persistent DATA loss with an optimistic Request can recover from SF8 to SF10 | Production state machines over fake radio. The second Client delivers every frame. |
| Repeated Grant loss cannot retain an unconfirmed higher SF | All three rules roll back their trial. With two SF10 reservations, an eight-Client test also rejects a third Client's recovery probe without interrupting peer delivery. |
| A previous SF8 delivery with a lost ACK remains useful after a control outage | The first valid same-session Request gets a trial at the last accepted SF if it fits. Verified over fake radio. |
| A pending downshift expires when report review stops | Native policy tests cover the frame deadline and a lock count that saturates below it. |
| A local final ACK timing fault preserves the recorded delivery without training ADR | Native fault injection. |
| A Gateway stall across one Client's primary RX slot leaves the other Client's ADR active | Fake radio and delayed Gateway loop. Both Clients delivered, one by retry. |
| The capture tool records nine streams with separate timestamps and hashes | Simulated serial ports. |

[Capacity matrix](SCHEDULED_LINK.md#capacity-at-15-seconds) · [Build and test records](BUILD_MEMORY.md)

## Experimental ADR model

`python tools/adr_sim.py --seed 42` ran 128 paired trials of 480 frames
per scenario, rate and rule. The model assumes one Client, perfect SF9
control, independent per-attempt interference and a provisional decode
curve. Loss is measured after up to four DATA attempts.

| Selected model scenario | Legacy | EWMA | Kalman |
|---|---:|---:|---:|
| 20% independent interference, time above reference SF | 78.36% | 0.00% | 0.00% |
| Same case, final loss | 0.174% | 0.179% | 0.179% |
| 9 dB blockage with 10% interference, mean time to reference SF | 71.1 s | 105.5 s | 105.5 s |
| Same blockage case, final loss | 0.695% | 0.960% | 0.960% |

The reference is the lowest SF that meets the model's provisional 50%
decode threshold. Each policy starts the synthetic blockage with fresh
state at SF7, and the decode curve stays fixed when a policy threshold
changes. In these cases, the experimental rules spent less time above
the reference SF and reached it later after blockage. Calibrating the
Request-to-DATA SNR offsets requires matched field runs.

In the same blockage case, lowering only the policy thresholds by 2.5 dB
raised the mean response to 104.3 s for Legacy, 282.3 s for EWMA and
274.7 s for Kalman. All 128 trials per rule still reached the reference
SF with the recovery search. The shift equals one step between the
provisional SF thresholds. An optimistic threshold can still delay
recovery when occasional DATA delivery breaks the complete-miss run.

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
The running firmware serves a single roster of eight positions. For a
future rotation at 32 B and 15 s per group, the model gives one scheduled
opportunity per Client every 135 s with eight SF7 or SF8 positions per group,
150 s with seven SF9 positions, and 360 s with three SF10 positions.
Mixed groups depend on their SF reservations. Loss, retry contention,
outages, and sample generation can increase the gap between fresh readings.

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
With modeled SF10 control, homogeneous DATA SF9 and SF10 groups shrink to
four and two respectively. Rotation stretches to 270 s and 525 s in those
cases, before losses. This reinforces keeping SF9 control for the current profile.

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
