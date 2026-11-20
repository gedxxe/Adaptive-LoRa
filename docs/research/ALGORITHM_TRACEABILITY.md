# Research-to-code traceability

The [four references](../references/README.md) and [figure register](../references/FIGURE_SOURCE_REGISTER.md)
identify the papers, figures and rights. The [v2 wire and timing specification](../en/SCHEDULED_LINK.md)
defines the shipped research profile. The default firmware uses the v1
HELLO/ACK path. The [airtime feasibility study](../en/TDMA_FEASIBILITY.md)
is an earlier analytical comparator.

| Paper and mechanism | Adaptation in Adaptive-LoRa | Source and host evidence |
|---|---|---|
| R1, TS-VP-LoRa, §§3.1–3.3, Figs. 1–3, pp. 148–150. The network server schedules payload/SF groups and the gateway advertises frame timing. Packet airtime and guards determine slot length. | The gateway grants index 0 to client 2. The two roles anchor locally at serviced beacon completion and derive one serialized SF7 calendar from the payload budget. The client skips a missed beacon or expired launch window. Parallel SF planes and channel hopping were excluded. | [`ScheduledLink::gateway`, `acceptBeacon` and `client`](../../src/app/scheduled_link.cpp); [`Schedule` and `airtimeUs`](../../src/common/protocol/scheduled_protocol.h). Tests include `timing_and_payload_bounds`, `happy_path_stays_inside_slot_and_repeats`, `invalid_beacon_and_missing_beacon_suppress_transmit`, and `stale_beacon_cannot_rearm_and_expired_launch_is_skipped`. |
| R3, LoRaHART, §§3, 5–6, Figs. 1–3, pp. 10:6, 10:9–10:10. Beacon, uplink, multicast bitmap and retransmission are timed around the gateway’s radio use. Node position in the bit vector records reception. | One SX1262 serializes uplink and feedback. The bitmap position is the schedule index. The feedback binds to network, gateway boot, frame, schedule, round and accepted client boot/sequence. A zero bit and an expired feedback window have separate results. The initial transmission and three retry opportunities reuse the same DATA bytes. The gateway accepts once per frame and re-acks a duplicate. SX1301 concurrency, high-SF multicast and opportunistic CSMA were excluded. | [`ScheduledLink::gateway` and `client`](../../src/app/scheduled_link.cpp); [`validBitmap`, `encode`, `decode`](../../src/common/protocol/scheduled_protocol.cpp). Tests cover lost DATA, lost ACK, duplicate DATA, wrong identities, retry exhaustion and bitmap padding for 1/10/70 positions. Only the single-client wire format runs end to end. |
| R2, Soy, §§3–4, Figs. 4–6. Nodes estimate link conditions from beacon RSSI and select SF locally with distributed slot choices. | The gateway remains the future ADR authority. Read RX RSSI/SNR with delivery and retry history, apply hysteresis over a minimum observation window, and activate versioned changes at an agreed frame boundary. Node-local SF choice and random slot allocation were excluded. | [`RadioDriver::poll`](../../src/common/radio/radio_driver.cpp) collects metrics. ACK outcome and retry records exist in the scheduled profile. The evaluator, thresholds, hysteresis and PHY commit handshake remain planned. Both profiles run SF7. |
| R4, ATS-LoRa, §III and Fig. 6, p. 12290. The model relates frame size to distribution of nodes among scheduled channel/SF groups. | Compare frame-budget sensitivity to payload sizes and workload. The deployed research profile has a fixed per-configuration period and no geographical assignment. | [`tdma_feasibility.py`](../../tools/tdma_feasibility.py) sizes host-only heterogeneous workloads. [`Schedule::valid`](../../src/common/protocol/scheduled_protocol.h) enforces the distinct v2 calendar budget. No adaptive frame-size controller or location-based allocation is implemented. |

The [scheduled tests](../../test/test_scheduled_link/test_main.cpp) run actual
role code with fake radio and clock inputs. Tests include client/gateway
reboot, rejection of retired sessions, late beacon TX completion, rollover
of the driver's 32-bit timer, and deliberate separation of v1 and v2 frames.
`FirmwareRuntime` selects the v2 role path in the ESP32 research build.
Section timing and equations are in the [v2 specification](../en/SCHEDULED_LINK.md).
The [repository audit](ITERATION_019_AUDIT.md) lists configuration,
compatibility, provenance and verification findings.

Measure radio turnaround, IRQ servicing, oscillator drift, supply headroom
and BUSY/flash blocking before treating the experimental guard as an RF
bound. Future rejoin messages, multi-client mapping, replay hardening and a
general application delivery API require separate acceptance work.
