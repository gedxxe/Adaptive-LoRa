# Adaptive-LoRa

The firmware baseline is main at `541e2e8b985f7cd67ca4d1f1af323c46aed4333e`,
which includes the V3.4 bench preparation from [PR #6](https://github.com/gedxxe/Adaptive-LoRa/pull/6).
The [reference review](docs/references/README.md) and [research-to-code matrix](docs/research/ALGORITHM_TRACEABILITY.md)
trace the four primary papers to the proposed design and current tests.

**Research title:** Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks.

Adaptive-LoRa is an independent research and embedded firmware project for private, single-radio LoRa peer-to-peer networks. Industrial energy monitoring is a general research use case; the protocol does not require a particular meter, fieldbus, backhaul, database or deployment. This is not LoRaWAN.

## Research scope and current status

The default `heltec_wifi_lora_32_V3_multiclient` build runs protocol v3 with eight
clients, exact-length DATA slots, aggregate feedback and deterministic
retries. The M2 HELLO/ACK and v2 single-client profiles remain available for
comparison. The gateway records per-client uplink evidence for offline ADR
analysis. All nodes in the v3 network use the same fixed PHY.
See the [startup guide](docs/en/STARTUP_AND_PROVISIONING.md) and [wire specification](docs/en/SCHEDULED_LINK.md).

| Evidence | Current baseline |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `3` |
| ESP32 compilation and native tests | See the [current verification record](docs/en/PROGRESS.md) |
| Implemented | v3 TDMA, exact-length grants, aggregate feedback and shared retries |
| Capture, model and documentation verification | See [build memory](docs/en/BUILD_MEMORY.md) |
| Physical NVS/reboot/SPI/IRQ/RF/antenna | NOT VERIFIED; M0–M2 acceptance IN PROGRESS |

Host integration tests use the actual client/gateway state machines with bounded fake transport, clock and serial adapters. Simulated loss, delay, object recreation, radio-operation failures and IRQ inputs are not physical measurements. See the [driver audit and coverage](docs/en/RADIO_DRIVER_AUDIT.md). The [progress snapshot](docs/en/PROGRESS.md), [software audit](docs/en/SOFTWARE_AUDIT.md) and [build memory](docs/en/BUILD_MEMORY.md) record exact evidence and limitations.

## Hardware and provisional PHY

The bench pair is two Heltec WiFi LoRa 32 V3.4 boards with ESP32-S3, SX1262
and stock monopoles. The owner confirms the 902–928 MHz procurement option
and V3.2 wiring, with V3.4 changes to SMT implementation and the crystal.
Use the V3.2 schematic as the electrical reference. Physical SKU labels,
antenna characteristics and peripheral behavior still need bench checks.
See [HARDWARE.md](docs/en/HARDWARE.md).

RadioLib is pinned to **7.8.1**. The adapter uses the board's GPIO8-14 radio
interface, a 1.8 V DIO3 TCXO supply and DC-DC regulator selection. Serial
runs through CP2102 at 115200; native USB CDC is disabled. The fixed research
profile remains 915.0 MHz, BW125, SF7, CR4/5, preamble 8, explicit header,
PHY CRC, private sync 0x12, 22 dBm requested TX power and 1 MHz SPI.
Both profiles use SF7 throughout. SF9 remains reserved. Verify conducted
power and supply margin in an authorized RF setup before radiated trials.

The 915 MHz Moxon is the design baseline; fabrication, gain and matching have
not been measured. Read [HARDWARE.md](docs/en/HARDWARE.md) and the
[migration audit](docs/en/HARDWARE_MIGRATION_AUDIT.md) before powering a board.
Earlier hardware descriptions remain in historical records and Git history.

## Architecture principles

One image loads a validated `GATEWAY` or `CLIENT` role from NVS. Missing, corrupt or unsupported configuration enters provisioning mode with application TX disabled. Role edits apply only after save and controlled reboot; identical hardware is not used to infer roles.

`RadioTransport` separates the application state machines from driver policy.
`RadioDriver` uses one `RadioHardware` reference, while `Sx1262Hardware`
owns all RadioLib, SPI and radio GPIO access. `Esp32Firmware` constructs
hardware/storage adapters before the runtime that references them.

Native tests execute the actual runtime, driver, configuration store, apps,
console and logger. Small SDK shims supply simulated time, serial input,
reset and random values. They test software decisions and error handling,
while flash durability, physical reset and hardware entropy still require
board tests. See the [runtime audit](docs/en/RUNTIME_AUDIT.md),
[storage audit](docs/en/CONFIG_STORE_AUDIT.md) and current migration report.

Session generation permits eight raw calls. If no valid identity is
available, applications suppress TX, log SESSION_ERROR and retry at
one-second intervals. The existing timestamp corrections and open SDK
wait/allocation limits remain. `main.cpp` contains only construction and
setup/loop delegation in 15 lines.

Four target boundaries are documented in [ARCHITECTURE.md](docs/en/ARCHITECTURE.md): protocol core, radio adapters, platform services and an application payload/event interface. Platform services are currently ESP32/Arduino-specific; a general application submission/delivery API is still absent. The protocol core contains no Modbus, meter-model, Wi-Fi/MQTT, PostgreSQL or dashboard integration. Do not treat the target architecture as fully implemented.

## Repository structure

```text
Adaptive-LoRa/
├── include/             # Provisional pins, fixed PHY and project limits
├── src/app/             # ESP32-S3 runtime, NVS and bounded console
├── src/common/protocol/ # Frame codec and sequence/session helpers
├── src/common/radio/    # Transport contract and SX1262 adapter
├── src/common/config/   # Pure configuration model/encoding
├── src/common/diagnostics/
├── src/gateway/         # Production gateway state machine
├── src/client/          # Production client state machine
├── test/                # Native unit/integration tests and bounded fakes
├── docs/en/             # Canonical engineering record
├── docs/zh-CN/          # Simplified Chinese companion
├── logs/                # Sanitized build/test history and evidence
└── tools/               # Bench capture, consistency checks and host-only research
```

## Build and tests

Install PlatformIO Core and a native C++17-capable host toolchain, then run from the repository root:

```sh
pio run -e heltec_wifi_lora_32_V3_multiclient
pio test -e native
pio test -e native_multiclient
```

The firmware uses `heltec_wifi_lora_32_V3` with Arduino and RadioLib 7.8.1.
The installed platform 55.03.311 already supports the board; the migration
keeps Arduino 3.3.11/IDF 5.5.5 and uses Xtensa GCC 14.2.0 rather than changing
the platform. Core 6.1.19, native 1.2.1, Unity 2.6.1 and host GCC 6.3.0 are
recorded in the build evidence. The platform remains unpinned, so preserve
resolved manifests for reproducibility. Separate capture tests use PlatformIO
Python: `python -m unittest discover -s tools -p test_capture_bench.py -v`.
Run the additional host checks separately:

```sh
python -m unittest discover -s tools -p test_tdma_feasibility.py -v
python -m unittest discover -s tools -p test_check_consistency.py -v
python tools/check_consistency.py
```

The checker derives current versions and the firmware environment from source files;
it excludes archived records. The analytical scenario report contains 216 cases
with unmeasured timing costs left explicit. Neither is a physical test.

## Provisioning and future bench procedure

At 115200 baud, use `help`, `status`, `config show`, the `config set`
commands, `config save` and `reboot`. `hello-interval-ms` defaults to
3,000 ms. M2 waits it after the transaction completes, so a lost ACK adds
the ACK wait and transmit time. Legacy v2 uses the saved value as its
period. v3 uses it for the report source and derives its own feasible
superframe from the gateway roster. Existing NVS values persist until changed.

For the M2 link check, run `config set hello-interval-ms 3000` on **both** boards,
save, reboot and confirm the loaded value with `status`. The gateway uses its
interval for display freshness. Initial IDs (network/node/gateway) are gateway
`1/1/1` and client `1/2/1`. Set all IDs explicitly; role edits preserve IDs. `config show` is staged state, `status` is loaded state. Network IDs are 1–65535; node/gateway IDs are 1–65534. Start with one-board Windows enumeration and private same-board ROM backups.
See the [bench runbook](docs/en/BENCH_RUNBOOK.md) and [run-record template](logs/test/RUN_RECORD_TEMPLATE.md). Physical acceptance is pending. Radiated tests require an
authorized RF setup.

## Development roadmap

1. Finish software verification and resolve dependency timing/allocation assumptions within M0–M2.
2. Verify each delivered board, then measure NVS/reboot, pins, supply, IRQ, display timing and the authorized two-device HELLO/ACK link.
3. Measure radio transitions and clock drift with the v3 research build, then verify the guard, shared retries and loss recovery before expanding the roster.
4. M4–M6: bitmap/selective retry, versioned conservative ADR and synchronization recovery, with fault tests before physical acceptance.
5. M7–M8: controlled reference-vs-Moxon evaluation and explicitly labelled logical-node simulations.
6. Add application payload/event and platform-service interfaces in a separate reviewed task before integrating other embedded applications.

The [milestone plan](docs/en/MILESTONE_TASKS.md) and [test plan](docs/en/TEST_PLAN.md) define acceptance criteria. The [TDMA feasibility notes](docs/en/TDMA_FEASIBILITY.md) retain the earlier host sizing model and its assumptions.

## Research and reproducibility

Preserve chronology, failed attempts, error resolutions, exact commands, package versions and the distinction between compile, host simulation and physical evidence. Historical records are sanitized publication copies: identity/path labels were redacted, not results reclassified. Old hashes identify original artifacts; see [IDENTITY_AUDIT.md](docs/en/IDENTITY_AUDIT.md) and [logs guidance](logs/README.md).

Use general application payloads; keep integrations outside protocol logic. Publish assumptions, controlled test conditions and limitations. Do not claim link capacity, antenna gain or hardware validation without evidence. Do not redistribute full research papers without permission. Third-party dependencies remain external; [THIRD_PARTY.md](THIRD_PARTY.md) records their licenses. No project-wide redistribution license has been selected in this task.

Read [AGENTS.md](AGENTS.md) before changes. English is canonical; [简体中文](README.zh-CN.md) is the secondary documentation language.

Physical preparation: [M0-M2 readiness](docs/en/PHYSICAL_ACCEPTANCE_READINESS.md) links the integrated-radio board and pair gates, capture/CSV rules and remaining ERR-021/SDK-startup holds. No physical procedure has been executed; use a new evidence directory and preserve private backups before upload/config changes.
