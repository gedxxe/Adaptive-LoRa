# Results and research

[Home](../../README.md) · [Field test](BENCH_RUNBOOK.md) · [Protocol](SCHEDULED_LINK.md) · [简体中文](../zh-CN/PROGRESS.md)

<!-- current-state:start -->
| Check | Result |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `5` (default) · `6` (rotation) |
| Board | Heltec WiFi LoRa 32 V3.4, ESP32-S3/SX1262 |
| Adaptive image | `heltec_wifi_lora_32_V3_adaptive`, ESP32 build passed. Native cases 46/46, including all three ADR rules. |
| Rotating image | `heltec_wifi_lora_32_V3_rotating`, ESP32 build passed, 8/8 native cases. Authenticated wire and physical tests pending. |
| Fixed-SF image | `heltec_wifi_lora_32_V3_multiclient`, ESP32 build passed, 15/15 runtime cases |
| Shared tests | 356/356 native and 66/66 Python cases passed |
| Toolchain | PlatformIO 6.1.19, Espressif32 55.3.311, Arduino ESP32 3.3.11, Xtensa 14.2.0+20260121, RadioLib 7.8.1 |
| Field test | Walk test and radio timing measurements pending |
<!-- current-state:end -->

The operator reported SF adaptation through SF10 in an earlier two-board
range trial. The current profiles still need field tests with 32 B samples.

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
| An observed primary miss still guides recovery after the last ACK has a timing fault | Tests run the production state machines on a fake radio with two and eight Clients. One Client reaches SF10 and every stationary peer delivers each frame. |
| A local primary RX error cannot train that Client's policy or erase a peer's primary miss | Fault injection. The affected Client stays at SF7 and the peer reaches SF8. |
| A successful delivery between two misses allows a higher-SF trial | A miss–success–miss fixture with strong Request SNR reaches SF8 at the next Grant. |
| Failed SF trials release airtime while the Clients still send Requests | Eight-Client fault injection. Two Clients fail at every DATA SF. A third reaches SF10, and the other five deliver in every frame. |
| A confirmed SF survives the last ACK timing fault beyond the trial deadline | Native runs with two and eight Clients continue delivery after the normal deadline. A separate check rejects mismatched and pre-trial frame receipts. |
| Low RSSI with good delivery can stay at SF7 | A paired fixture keeps a reliably delivering weak-RSSI Client at SF7 while its peer with stronger RSSI and DATA loss reaches SF10. |
| The capture tool records nine streams with separate timestamps and hashes | Simulated serial ports. |

[Capacity matrix](SCHEDULED_LINK.md#capacity-at-15-seconds) · [Build and test records](BUILD_MEMORY.md)

## Experimental ADR model

`python tools/adr_sim.py --seed 42` ran 128 paired trials of 480 frames
per scenario, rate and rule. The model assumes one Client, perfect SF9
control, independent per-attempt interference and a provisional decode
curve. Loss is measured after up to four DATA attempts.

| Selected model scenario | Fixed SF7 | Delivery window | EWMA | Kalman |
|---|---:|---:|---:|---:|
| 20% independent interference, time above reference SF | 0% | 78.54% | 0.06% | 0.06% |
| Same case, final loss | 0.179% | 0.174% | 0.179% | 0.179% |
| 9 dB blockage with 10% interference, mean time to reference SF | Never reached | 59.2 s | 81.1 s | 81.1 s |
| Same blockage case, final loss | 32.196% | 0.618% | 0.802% | 0.802% |
| Same blockage case, mean attempts per report | 2.057 | 1.137 | 1.183 | 1.183 |

The reference is the lowest SF that meets the model's provisional 50%
decode threshold. Each policy starts the synthetic blockage with fresh
state at SF7, and the decode curve stays fixed when a policy threshold
changes. In these cases, the experimental rules spent less time above
the reference SF and reached it later after blockage. Calibrating the
Request-to-DATA SNR offsets requires matched field runs.

In the same blockage case, lowering only the policy thresholds by 2.5 dB
raised the mean response to 63.3 s for the delivery window, 92.2 s for
EWMA and 92.1 s for Kalman. All 128 trials per rule reached the reference
SF. The shift equals one step between the provisional SF thresholds.
The model assumes ideal control. The native fault tests above separately
exercise timing faults and peer traffic.

## Papers and comparison runs

[Research-to-code map](../research/ALGORITHM_TRACEABILITY.md) ·
[Paper references and figure sources](../references/README.md)

| Use | PlatformIO environment | Wire version |
|---|---|---:|
| Adaptive field test and PlatformIO default | `heltec_wifi_lora_32_V3_adaptive` | 5 |
| Rotation study with up to 128 registered IDs | `heltec_wifi_lora_32_V3_rotating` | 6 |
| Earlier fixed-SF comparison | `heltec_wifi_lora_32_V3_multiclient` | 3 |
| Single-Client TDMA comparison | `heltec_wifi_lora_32_V3_research` | 2 |
| HELLO/ACK comparison | `heltec_wifi_lora_32_V3` | 1 |

Use the same wire version on every board in a run. The [matched SF7 Gateway](BENCH_RUNBOOK.md#check-the-pair-and-walk-the-route)
keeps the adaptive wire format, control PHY, period and retry policy. Keep
antenna placement, offered load and route fixed between runs. The older
fixed-SF image also changes the control PHY and frame timing.
`PROTOCOL_VERSION` tracks the HELLO/ACK image. The other wire versions
are selected by the build. All images share `VERSION` and NVS schema 1.

<a name="research-target-for-70-clients"></a>

## Extended Client capacity

The wire-v6 scheduler supports a registry of 128 Clients. One radio serves up to eight per frame and may select fewer when DATA uses more airtime. At a 32-byte ceiling, the production selector schedules 70, 84 and 128 homogeneous nodes across the periods in the [capacity table](SCHEDULED_LINK.md#rotating-schedule). Security adds overhead. For example, the modeled 128-node case at SF10 goes from 645 s to 960 s once authenticated framing is budgeted. The secure framing is still a design.

The 5–10 minute objective is a service interval for suitable SF distributions. A delivered report additionally requires SF9 control coverage, successful DATA and the relevant feedback. Measure the longest gap for each node and report the conditions behind any missed deadline.

1. Capture repeated runs with 1, 4 and 8 physical Clients and one Gateway. Record first-attempt and final delivery, retransmissions, shared-retry use, SF changes, timing faults, reset recovery and per-Client arrival gaps. Keep sampling timestamps if reporting age of information.
2. Compare fixed SF7, the delivery-window policy, EWMA and Kalman using the same traffic, loss, control and airtime assumptions. The existing model uses one Client and ideal control. Extend it to correlated losses, blocked links and group rotation before drawing conclusions at 70–128 nodes.
3. Prototype authenticated control and encrypted DATA, then remeasure frame margins and Gateway transmit airtime. Test credential provisioning, restart, replay, RF interference and backup restoration before a deployment decision. Treat a single Gateway failure as a network outage.
4. Compare against matched scheduling and contention baselines before making a capacity claim. Record the antenna pattern and traffic assumptions. Directional coverage from a Moxon and concurrent reception on a concentrator describe different deployment conditions.

## Past work

[Earlier manuals, hardware checks and test plans](https://github.com/gedxxe/Adaptive-LoRa/tree/4ab3655dc8ca3b7f9ddc382d8ebeeeadceb6d2c7/docs).
This pinned Git snapshot keeps the documents from before consolidation.
The project also keeps its [build log](BUILD_MEMORY.md),
[decisions](DECISIONS.md) and [error log](ERROR_LOG.md). The
[log index](../../logs/README.md) explains which files are historical.
