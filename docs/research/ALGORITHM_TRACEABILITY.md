# Research-to-code map

Adaptive-LoRa combines payload-aware TDMA, aggregate feedback and scheduled
retries on one half-duplex SX1262. The implementation uses a fixed PHY for
the v3 link and evaluates ADR candidates offline. The
[four references](../references/README.md) describe the experimental systems
that informed each decision.

| Reference | What Adaptive-LoRa implements | Evidence and remaining difference |
|---|---|---|
| **TS-VP-LoRa**, §§3.1–3.3 | A beacon announces the roster. Exact-length requests determine serial DATA slots. DIO1 completion timestamps anchor the schedule. [MAC implementation](../../src/app/multiclient_link.cpp) | Host cases delay event handling by 30 ms. Actual turnaround and clock drift still need measurement. Our single receiver uses one serialized calendar. The paper also considers parallel SF planes and channel hopping. |
| **LoRaHART**, §§3, 5–6 | Cumulative bitmap acknowledgement, up to four shared slots per retry round, and duplicate suppression. [Link tests](../../test/test_multiclient_link/test_main.cpp) | Host tests exercise all eight Clients with 64-byte payloads, including loss and retry. Our Gateway serializes RX and TX. LoRaHART uses concentrator scheduling and contention-based recovery. |
| **ATS-LoRa**, §III | Worst-case roster admission sets the advertised period. Exact-length grants determine the active slot lengths. [Capacity model](../../tools/model_multiclient.py) | The eight-Client roster fits the calculated budget. Larger rosters are analytical comparisons. Geographic SF groups and autonomous frame reallocation remain reference mechanisms. |
| **Soy**, §§3–4 | Collect Gateway-side RSSI/SNR and first, retry or missing delivery per Client. Evaluate candidate SF with experiment-defined thresholds and hysteresis. [Offline evaluator](../../tools/adr_shadow.py) | The reference uses node-local downlink RSSI. Our evaluator uses uplink observations; v3 does not change PHY over the air. |

[View the v3 superframe and bitmap illustration](figures/v3-superframe.svg).

## Reproduce the software evidence

- [Runtime tests](../../test/test_multiclient_runtime/test_main.cpp) cover the
  actual driver, delayed completion handling, and resume after an outage.
- [Progress and validation](../en/PROGRESS.md) lists tested configurations,
  build results and physical gaps.
- [Protocol reference](../en/SCHEDULED_LINK.md) contains the wire layout,
  slot calculation, retries and inputs for the offline evaluator.

## Close the research gaps

1. Measure packet delivery, feedback and recovery with 1, 2, 4, then 8
   Clients. Compare first-pass delivery, delivery after retries and
   utilization with observed timing misses and drift.
2. Measure SX1262 retune and re-arm time. Evaluate proposed SF changes
   against the slot budget before designing a versioned activation and
   recovery exchange.
3. Keep the 70-node analysis separate. A roster or grant for that scale
   needs a different control encoding and a capacity study.

The [Iteration 019 audit](ITERATION_019_AUDIT.md) records the earlier
single-Client work. The [figure register](../references/FIGURE_SOURCE_REGISTER.md)
documents provenance and reuse terms.
