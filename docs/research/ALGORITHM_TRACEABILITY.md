# Research-to-code traceability

The [reference review](../references/README.md) identifies the research
mechanisms. The [figure register](../references/FIGURE_SOURCE_REGISTER.md)
ties them to the exact local PDFs. [PROTOCOL.md](../en/PROTOCOL.md) is the
wire specification. [TDMA_FEASIBILITY.md](../en/TDMA_FEASIBILITY.md) holds
candidate design details and the analytical model.

The status describes Adaptive-LoRa production behavior. **Implemented**
means the described path is present in the firmware. **Partially implemented**
means a production prerequisite exists and the stated feature is incomplete.
**Planned** means the proposed mechanism is absent from production, even
when host arithmetic or negative-version tests support it. **Not adopted**
means the research mechanism is intentionally outside the chosen design.

## Mechanism matrix

Test names in this table identify existing guardrails. The last column
applies to the feature named in the first column.

| Mechanism | Research reference | Original figure or section | Adaptive-LoRa adaptation | Current source and symbols | Current tests and what they cover | Status |
|---|---|---|---|---|---|---|
| Gateway beacon synchronization | R1, R2, R3 | R1 Figs. 1–3, §§3.1–3.3. R2 Fig. 4, §3.2. R3 Fig. 2, §5. | Use local TX/RX completion anchors and a gateway session, frame ID, schedule version, relative offset and lease. A missed beacon suppresses that frame’s DATA. | [message_types.h](../../src/common/protocol/message_types.h) reserves `BEACON`. [frame_codec.cpp](../../src/common/protocol/frame_codec.cpp) `encodeFrame` and `decodeFrame` reject it. | [test_frame_codec](../../test/test_frame_codec/test_main.cpp) `test_type_and_future_types` checks rejection. No beacon synchronization test. | Planned |
| TDMA scheduling | R1, R2, R3 | R1 Fig. 2, §3.2. R2 Fig. 4, §3.2. R3 Fig. 2, §§5–6. | Gateway assigns serialized slots on one SX1262. Start with one client. Measure drift and radio preparation before setting guards. | [tdma_feasibility.py](../../tools/tdma_feasibility.py) `schedule_budget` totals costs. No firmware schedule table or slot service. | [test_tdma_feasibility.py](../../tools/test_tdma_feasibility.py) `test_exact_superframe_boundary_and_one_microsecond_over` checks budget arithmetic. No firmware slot boundary test. | Planned |
| Superframe organization | R1, R3, R4 | R1 Figs. 1–2, §3.2. R3 Fig. 2, §5. R4 Fig. 6, §III-B. | Separate beacon, serialized DATA, aggregate feedback and deterministic retry budgets. Account for frame size, phase switching and guards. | Host `schedule_budget` and `scenario_report`. No phased production scheduler. | [test_tdma_feasibility.py](../../tools/test_tdma_feasibility.py) `test_unknown_is_not_zero_or_a_fit_claim` and `test_scenario_grid_is_complete_and_deterministic`. | Planned |
| Runtime PHY switching | R2, R3 | R2 Figs. 4–6, §§3.2–4. R3 Fig. 1, §§3, 5. | Use fixed control SF9/BW125 and a scheduled data profile in SF7–SF9/BW125. A profile change needs an explicit boundary and measured settle time. | [radio_config.h](../../include/radio_config.h) `phy::dataSf`, `phy::controlSf`. [sx1262_hardware.cpp](../../src/common/radio/sx1262_hardware.cpp) `Sx1262Hardware::begin` selects SF7 once. No profile switching path. | [test_sx1262_adapter](../../test/test_sx1262_adapter/test_main.cpp) verifies static initialization only. | Planned |
| ACK bitmap | R3 | Fig. 3, §5. | Bind to gateway/session/frame/schedule, use schedule order and LSB-first bits, validate length and padding, and distinguish MISSING from ACK_UNKNOWN. | `ACK_BITMAP` reserved and rejected by the codec. Host `schedule_budget` accounts for bitmap frame size. | `test_type_and_future_types` checks rejection. `test_complete_bitmap_size_and_control_phy` checks host byte cost. No production bitmap parsing test. | Planned |
| Selective retransmission | R3 | Fig. 2, §5. | Eligible clients reuse packet identity/payload only in assigned retry slots. Reserve a bounded budget and diagnose exhaustion. | [sequence_tracker.cpp](../../src/common/protocol/sequence_tracker.cpp) `SequenceTracker::observe` supplies bounded, volatile HELLO duplicate handling. No DATA retry scheduler or retry counter. | [test_app_integration](../../test/test_app_integration/test_main.cpp) `test_same_packet_replay_preserves_identity_and_reacks_once` injects a replay. Host `test_retry_budget_includes_ack_each_round` checks cost. Neither exercises a production retry. | Planned |
| Conservative ADR | R2 | Figs. 5–6, §§3.2, 4. | Gateway evaluates rolling per-client observations with hysteresis, then uses a versioned proposal/accept/commit. Thresholds and windows require bench evidence. | [radio_driver.cpp](../../src/common/radio/radio_driver.cpp) `RadioDriver::poll` captures RX metrics. `CONFIG_PROPOSE/ACCEPT/COMMIT` reserved and rejected. No decision engine. | `test_type_and_future_types` checks rejection. [test_oled_telemetry](../../test/test_oled_telemetry/test_main.cpp) checks accepted-ACK metrics. No ADR transition test. | Planned |
| Synchronization recovery | R1, R3 | R1 §§3.1–3.3. R3 Fig. 2, §§5, 7.4. | Suppress DATA without a current beacon. Later rejoin on the common control PHY after lease loss or gateway session change. | `REJOIN_REQUEST/GRANT` reserved and rejected. `SequenceCounter::renew` and the M2 app recovery paths manage current HELLO/ACK identity and RX faults only. | `test_type_and_future_types` checks rejection. [test_runtime_reliability](../../test/test_runtime_reliability/test_main.cpp) `test_client_rollover_failure_preserves_identity_until_recovery` checks an M2 session fault. No synchronization/rejoin test. | Planned |
| Single-owner half-duplex operation | R3 | Figs. 1–2, §§3, 5. | One driver and one integrated radio. Main loop consumes IRQ events and serializes RX, TX, cleanup and recovery. | [firmware_runtime.cpp](../../src/app/firmware_runtime.cpp) `FirmwareRuntime::loop`, `RadioDriver::receive`, `transmit`, `poll`, `Sx1262Hardware::onDio1`. | [test_radio_driver](../../test/test_radio_driver/test_main.cpp) `test_repeated_rx_tx_transactions_reset_events_and_deadlines`, `test_latched_irq_with_low_gpio_is_consumed` and `test_high_gpio_without_latched_irq_is_consumed`. | Implemented |
| Link-quality observation for later adaptation | R2 | Fig. 5, §§3.2, 4. | Observe valid HELLO RSSI/SNR at gateway and accepted ACK metrics at client. Future ADR needs rolling per-client evidence. | `RadioDriver::poll` and [GatewayApp::update](../../src/gateway/gateway_app.cpp), [ClientApp::finish](../../src/client/client_app.cpp). | `test_gateway_counts_hello_and_ack_events_separately`; `test_client_only_updates_metrics_on_accepted_ack_and_keeps_accounting`. | Partially implemented |
| Geographic autonomous allocation and parallel packing | R4, R1, R2, R3 | R4 Figs. 1–4 and 6, §III. R1 Fig. 1, §3.2. R2 Fig. 4, §3.2. R3 Fig. 2, §6. | Use an explicit single-radio schedule. Indoor link behavior and airtime capacity need project measurements. | No coordinate allocation, random SF lane selection, parallel concentrator packing, or LMAC retry in the production tree. | No tests for these discarded mechanisms. | Not adopted |

## Client and gateway consistency audit

| Boundary | Finding from the current shared repository |
|---|---|
| Serialization and widths | Both roles call the same `encodeFrame` and `decodeFrame`. The header is 28 bytes, frame maximum 96 bytes, multibyte fields explicitly little-endian. The normal client HELLO is 28 bytes. ACK is 37 bytes with nine echo bytes. `test_golden_empty_hello_layout` and `test_ack_echo_layout_and_match` cover exact bytes. |
| IDs and types | Network 1–65535, distinct unicast endpoints 1–65534, nonzero sender boot ID and exact local destination are required. Only HELLO/ACK encode or decode in M2. Gateway accepts HELLO; Client in WAIT_ACK additionally requires the configured Gateway source and exact network/source/boot/sequence echo. Frame, schedule and config version fields are zero. There is no schedule validation path. |
| Timing and PHY | Runtime polls and then refreshes the monotonic timestamp before dispatch. Client starts the 1 s wait on serviced TX_DONE and rejects ACK at or after the deadline. Driver checks the two-second TX deadline before completion and uses wrap-safe comparisons. Both roles start on the same fixed SF7/BW125 profile. SF9 is reserved. No beacon, guard, slot or PHY transition executes. |
| Acknowledgments and retries | Gateway replies to a validated HELLO and can re-ACK a retained duplicate, distinguishing TX start and completion. A lost M2 ACK causes timeout, followed by a new logical transaction at the configured interval. M2 does not interpret bitmap positions or schedule a retry. |
| Session and duplicate limits | `SequenceCounter::take` exhausts before reuse and `renew` rejects a zero or repeated ID. `SequenceTracker::observe` keys network/source/boot and a 64-sequence window. New sessions replace an entry in round-robin order. Gateway reset discards history. Tests cover replay, old-session re-ACK, cache churn and 65,536-sequence exhaustion. This is a HELLO cache, not general anti-replay authentication or exactly-once DATA delivery. |
| Ownership and races | `Esp32Firmware` owns concrete adapters before runtime. DIO1 ISR sets a volatile latch. Poll consumes it with a pin-level fallback and validates hardware IRQ flags. RX is put in standby before the buffer is inspected and cleanup is checked. The single latch does not queue multiple arrivals. It and OLED-induced RX service latency still need direct timing evidence. |
| Recovery | Receive start failures are bounded by the existing app policy. Pending reboot gates new transmissions. ERR-021 remains open because RadioLib can block inside TX launch before the driver can service a deadline. No startup NVS preservation, IRQ timing, RF recovery or throughput was measured for this iteration. |

## Verification still needed for each production milestone

- **M3:** Review the one-client BEACON/DATA extension and compatibility
  policy before freezing bytes. Test reserved/active types, exact layout,
  truncated payloads, impossible schedules, stale session/frame/version,
  wraps, slot bounds and expiration. Measure DIO1-to-service, preparation,
  TX/RX turnaround and drift before selecting guards.
- **M4:** Test exact bitmap indexing, length and final-byte padding, unknown
  ACK versus explicitly missing reception, eligibility and retry exhaustion.
  Inject lost DATA, lost ACK, delayed retransmission, duplicate arrivals and
  gateway restart. Establish admission lifetime and retained identity rules
  before claiming duplicate-free DATA delivery.
- **M5:** Test transition conditions and hysteresis, lost proposal, accept
  and commit, and control-PHY recovery after mismatched profile state. Review
  the still-open common-control reconciliation after a lost commit before
  adding a second data PHY. Candidate thresholds are research inputs.
- **M6:** Test missed beacon expiry, stale schedules, reboot and explicit
  rejoin on the common control PHY. Quantify recovery after communication
  and radio failures.

The papers are literature evidence. The protocol proposals and airtime model
are design and analytical evidence. Native tests and firmware builds verify
software behavior. Measured SX1262 timing, RF delivery and hardware recovery
require separate bench captures. Do not transfer a paper’s figures or
experimental numbers into project measurements.
