# Research-to-code map

Adaptive-LoRa schedules DATA, feedback and retries on one SX1262.
Its [adaptive image](../en/SCHEDULED_LINK.md#adaptive-protocol-v4) fits
each Client's selected DATA SF into the same schedule. The
[four papers](../references/README.md) inform different parts of the design.

| Reference | What Adaptive-LoRa implements | Checks and limits |
|---|---|---|
| **TS-VP-LoRa**, §§3.1–3.3 | A beacon announces the roster. Exact-length requests determine serial DATA slots. DIO1 completion timestamps anchor the schedule. [MAC implementation](../../src/app/multiclient_link.cpp) | Host cases delay event handling by 30 ms. Actual turnaround and clock drift still need measurement. Our single receiver uses one serialized calendar. The paper also considers parallel SF planes and channel hopping. |
| **LoRaHART**, §§3, 5–6 | Cumulative bitmap acknowledgement, up to four shared slots per retry round, and duplicate suppression. [Link tests](../../test/test_multiclient_link/test_main.cpp) | Host tests exercise all eight Clients with 64-byte payloads, including loss and retry. Our Gateway serializes RX and TX. LoRaHART uses concentrator scheduling and contention-based recovery. |
| **ATS-LoRa**, §III | The Gateway checks every new SF against the roster's maximum payloads. Actual slots use the requested lengths. [Timing calculation](../../src/common/protocol/multiclient_protocol.cpp) | The walk test uses a fixed 15 s frame. The eight-Client check includes shared retry time. Geographic grouping and frame resizing are outside this experiment. |
| **Soy**, §§3–4 | Collect Gateway-side RSSI/SNR and first, retry or missing delivery per Client. v4 tries a lower SF when delivery and SNR support it. Degraded trials roll back. [Gateway policy](../../src/app/multiclient_link.cpp) | The reference uses node-local downlink RSSI. v4 uses uplink observations and serializes all PHY changes on one radio. |

[Architecture](figures/adaptive-architecture.svg) · [Frame sequence](figures/adaptive-frame.svg)

## Check the results

- [Runtime tests](../../test/test_multiclient_runtime/test_main.cpp) cover the
  actual driver, delayed completion handling, and resume after an outage.
- [Adaptive link tests](../../test/test_adaptive_link/test_main.cpp) cover
  mixed-SF traffic, ACK receipts, rollback and peer delivery in simulation.
- [Progress](../en/PROGRESS.md) lists build, host and physical test status.
- [Protocol reference](../en/SCHEDULED_LINK.md) contains the wire layout,
  slot calculation, retries and inputs for the offline evaluator.

## Close the research gaps

1. Measure packet delivery, feedback and recovery with 1, 2, 4, then 8
   Clients. Compare first-pass delivery, delivery after retries and
   utilization with observed timing misses and drift.
2. Measure SX1262 retune and re-arm time. Verify v4 Grant, profile trials,
   rollback and return to the common control profile with one moving Client
   and three stationary Clients. To isolate the ADR effect in a later
   comparison, keep control SF, period, load and retry policy matched.
3. Keep the 70-node analysis separate. A roster or grant for that scale
   needs a different control encoding and a capacity study.

The [Iteration 019 audit](ITERATION_019_AUDIT.md) records the earlier
single-Client work. The [figure register](../references/FIGURE_SOURCE_REGISTER.md)
documents provenance and reuse terms.
