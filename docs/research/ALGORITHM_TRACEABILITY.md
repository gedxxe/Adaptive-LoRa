# Research-to-code map

Adaptive-LoRa models one half-duplex SX1262 as a serial resource.
Its [adaptive image](../en/SCHEDULED_LINK.md#airtime-and-slot-budget) checks
each SF increase against airtime reserved for the full roster, including
retry slots. The [references](../references/README.md) give the context
for testing that policy.

| Reference | Relevant mechanism | Boundary in this project |
|---|---|---|
| **TS-VP-LoRa**, §§3.1–3.3 | Beacon synchronization and scheduling by payload length. | The project uses per-frame Request/Grant and serial DATA slots. The paper assigns slots using payload ranges and also studies parallel SF planes and channel hopping. |
| **LoRaHART**, §§3, 5–6 | Multicast ACK and in-frame recovery. | The project uses deterministic retry grants and serial RX/TX. LoRaHART studies concentrator scheduling and contention-based recovery. |
| **ATS-LoRa**, §III | Comparative study of autonomous frame sizing and geographic groups. | Context for future comparison. Geographic grouping and autonomous frame sizing are absent from the firmware. |
| **Soy**, §§3–4 | Choose an SF sufficient for link quality. | The paper uses local downlink RSSI. The Gateway uses uplink observations and limits each SF probe by the airtime reservation. |

[Architecture](figures/adaptive-architecture.svg) · [Frame sequence](figures/adaptive-frame.svg)

## Check the results

- [Runtime tests](../../test/test_multiclient_runtime/test_main.cpp) cover the
  actual driver, delayed completion handling, and resume after an outage.
- [Adaptive link tests](../../test/test_adaptive_link/test_main.cpp) cover
  mixed-SF traffic, interleaved first-attempt loss, same-session recovery,
  ACK receipts across an idle gap, rollback and peer delivery in simulation.
- [Progress](../en/PROGRESS.md) lists build, host and physical test status.
- [Protocol reference](../en/SCHEDULED_LINK.md) contains the wire layout,
  slot calculation, retries and the live [SF review](../en/SCHEDULED_LINK.md#adaptation-and-recovery).
  The [offline evaluator](../../tools/adr_shadow.py) uses its own window
  parameters for capture analysis.
- [Gateway ADR policy](../../src/app/adr_policy.cpp) keeps Legacy as the
  firmware default and separates SNR margin from loss evidence in two
  experimental rules. The [seeded model](../../tools/adr_sim.py) runs the
  same production decision function with ideal control and independent
  per-attempt interference. It isolates the link rule from multi-Client
  scheduling and provides no field result.

## Close the research gaps

1. Test whether SF admission preserves peer slot availability and improves
   delivery. Compare adaptive and fixed SF under matched period, control SF,
   load and retry policy using the [matched SF7 Gateway](../en/BENCH_RUNBOOK.md#check-the-pair-and-walk-the-route).
   Count an unobserved Gateway receive window separately from RF loss.
2. Test whether the serial airtime model predicts the occupied frame. Measure
   packet duration, SX1262 retune and RX re-arm, then explain residuals by
   host timing, transition time and clock error.
3. Compare the shared retry pool with a contention baseline on the same
   radio. Quantify control overhead against persistent slots for periodic
   traffic. Report delivery gaps and uncertainty by Client and test run.

For the [70-Client study](../en/PROGRESS.md#research-target-for-70-clients),
measure a physical subset and model grouped traffic. These experiments
can support a claim about where one radio is sufficient. They do not yet
establish a throughput or coverage advantage over a concentrator.

The [Iteration 019 audit](ITERATION_019_AUDIT.md) records the earlier
single-Client work. The [figure register](../references/FIGURE_SOURCE_REGISTER.md)
documents provenance and reuse terms.
