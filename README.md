# Adaptive-LoRa

Development baseline: `main` is canonical after the validated [Iteration 013 integration PR](https://github.com/gedxxe/Adaptive-LoRa/pull/3). Start future work from current `main`; preserve historical iteration branches and engineering records. See [PROGRESS.md](docs/en/PROGRESS.md) for validation evidence and publication status.

**Research title:** Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks.

Adaptive-LoRa is an independent research and embedded firmware project for private, single-radio LoRa peer-to-peer networks. Industrial energy monitoring is a general research use case; the protocol does not require a particular meter, fieldbus, backhaul, database or deployment. This is not LoRaWAN.

## Research scope and current status

The research direction is reliability-aware gateway scheduling and conservative link adaptation. **TDMA, ACK bitmap, selective retransmission scheduling and ADR are planned, not implemented.** The current delivery is the M0–M2 fixed-PHY HELLO/ACK baseline.

| Evidence | Current baseline |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `1` |
| ESP32 compilation | PASS |
| Native tests | 302/302 PASS: all 290 existing cases plus 12 production SX1262 adapter cases |
| Implemented | Explicit bounded frames, provisioned roles, HELLO/ACK identity checks, duplicate suppression, timeout/recovery and sequence/session handling |
| Physical NVS/reboot/SPI/IRQ/RF/antenna | NOT VERIFIED |

Host integration tests use the actual client/gateway state machines with bounded fake transport, clock and serial adapters. Simulated loss, delay, object recreation, radio-operation failures and IRQ inputs are not physical measurements. See the [driver audit and coverage](docs/en/RADIO_DRIVER_AUDIT.md). The [progress snapshot](docs/en/PROGRESS.md), [software audit](docs/en/SOFTWARE_AUDIT.md) and [build memory](docs/en/BUILD_MEMORY.md) record exact evidence and limitations.

## Hardware and provisional PHY

The sole active target is **Heltec WiFi LoRa 32 V3.2 HF** with ESP32-S3FN8,
SX1262, 8 MB flash and CP2102/UART0. Procurement is specified as the HF
902-928 MHz option; receipt and board markings have not been checked. No
PSRAM, OLED driver or battery-monitoring code is assumed.

RadioLib is pinned to **7.8.1**. The adapter uses the board's GPIO8-14 radio
interface, a 1.8 V DIO3 TCXO supply and DC-DC regulator selection. Serial
runs through CP2102 at 115200; native USB CDC is disabled. The fixed research
profile remains915.0 MHz, BW125, SF7, CR4/5, preamble8, explicit header,
PHY CRC, private sync0x12,10 dBm and1 MHz SPI. SF9 control remains reserved.

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
setup/loop delegation in15 lines.

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
└── tools/               # Repeatable bench-capture helper
```

## Build and tests

Install PlatformIO Core and a native C++17-capable host toolchain, then run from the repository root:

```sh
pio run -e heltec_wifi_lora_32_V3
pio test -e native
```

The firmware uses `heltec_wifi_lora_32_V3` with Arduino and RadioLib 7.8.1.
The installed platform55.03.311 already supports the board; the migration
keeps Arduino 3.3.11/IDF5.5.5 and uses Xtensa GCC14.2.0 rather than changing
the platform. Core6.1.19, native1.2.1, Unity2.6.1 and host GCC6.3.0 are
recorded in the build evidence. The platform remains unpinned, so preserve
resolved manifests for reproducibility. Separate capture tests use PlatformIO
Python: `python -m unittest discover -s tools -p test_capture_bench.py -v`.

## Provisioning and future bench procedure

Serial: 115200 baud. Commands: `help`, `status`, `config show`, `config set role gateway|client`, `config set network-id N`, `config set node-id N`, `config set gateway-id N`, `config save`, `reboot`. Optional `config set hello-interval-ms <1000..3600000>`; default 5000 ms after transaction completion.

Initial IDs (network/node/gateway) are gateway `1/1/1` and client `1/2/1`. Set all IDs explicitly; role edits preserve IDs. `config show` is staged state, `status` is loaded state. Network IDs are 1–65535; node/gateway IDs are 1–65534. See the deferred [bench runbook](docs/en/BENCH_RUNBOOK.md) and [run-record template](logs/test/RUN_RECORD_TEMPLATE.md); no upload or RF test has been performed.

## Development roadmap

1. Finish software verification and resolve dependency timing/allocation assumptions within M0–M2.
2. After procurement/assembly, verify NVS/reboot, pins/supply/IRQ and the two-device HELLO/ACK link.
3. M3: one-client beacon/TDMA after measuring radio transitions and clock drift.
4. M4–M6: bitmap/selective retry, versioned conservative ADR and synchronization recovery, with fault tests before physical acceptance.
5. M7–M8: controlled reference-vs-Moxon evaluation and explicitly labelled logical-node simulations.
6. Add application payload/event and platform-service interfaces in a separate reviewed task before integrating other embedded applications.

The [milestone plan](docs/en/MILESTONE_TASKS.md) and [test plan](docs/en/TEST_PLAN.md) define acceptance gates. No later MAC implementation is included in the hardware migration.

## Research and reproducibility

Preserve chronology, failed attempts, error resolutions, exact commands, package versions and the distinction between compile, host simulation and physical evidence. Historical records are sanitized publication copies: identity/path labels were redacted, not results reclassified. Old hashes identify original artifacts; see [IDENTITY_AUDIT.md](docs/en/IDENTITY_AUDIT.md) and [logs guidance](logs/README.md).

Use general application payloads; keep integrations outside protocol logic. Publish assumptions, controlled test conditions and limitations. Do not claim link capacity, antenna gain or hardware validation without evidence. Do not redistribute full research papers without permission. Third-party dependencies remain external; [THIRD_PARTY.md](THIRD_PARTY.md) records their licenses. No project-wide redistribution license has been selected in this task.

Read [AGENTS.md](AGENTS.md) before changes. English is canonical; [简体中文](README.zh-CN.md) is the secondary documentation language.

Physical preparation: [M0-M2 readiness](docs/en/PHYSICAL_ACCEPTANCE_READINESS.md) links backed-up MCU-only and pair gates, capture/CSV rules and remaining ERR-021/SDK-startup holds. No physical procedure has been executed; use a new evidence directory and preserve private backups before upload/config changes.

Iteration014 migrated software passes the S3 build,302 native tests and13 fake-serial capture tests. Protocol/NVS/app policy and main.cpp remain unchanged. ERR-021 still affects the shared radio dependency. Exact sources, GPIO/revision/API/test matrices and physical checks are in the migration audit.
