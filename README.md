# Adaptive-LoRa

**Research title:** Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks.

Adaptive-LoRa is an independent research and embedded firmware project for private, single-radio LoRa peer-to-peer networks. Industrial energy monitoring is a general research use case; the protocol does not require a particular meter, fieldbus, backhaul, database or deployment. This is not LoRaWAN.

## Research scope and current status

The research direction is reliability-aware gateway scheduling and conservative link adaptation. **TDMA, ACK bitmap, selective retransmission scheduling and ADR are planned, not implemented.** The current delivery is the M0–M2 fixed-PHY HELLO/ACK baseline.

| Evidence | Current baseline |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `1` |
| ESP32 compilation | PASS |
| Native tests | 290/290 PASS: all 266 existing cases plus 24 boot-ID/dependency-delay reliability cases |
| Implemented | Explicit bounded frames, provisioned roles, HELLO/ACK identity checks, duplicate suppression, timeout/recovery and sequence/session handling |
| Physical NVS/reboot/SPI/IRQ/RF/antenna | NOT VERIFIED |

Host integration tests use the actual client/gateway state machines with bounded fake transport, clock and serial adapters. Simulated loss, delay, object recreation, radio-operation failures and IRQ inputs are not physical measurements. See the [driver audit and coverage](docs/en/RADIO_DRIVER_AUDIT.md). The [progress snapshot](docs/en/PROGRESS.md), [software audit](docs/en/SOFTWARE_AUDIT.md) and [build memory](docs/en/BUILD_MEMORY.md) record exact evidence and limitations.

## Hardware and provisional PHY

- Current inventory: **2 × AI-Thinker RA-01SCH (LLCC68), 1 × ESP32-C3 Super Mini**. The second MCU and planned 915 MHz Moxon antenna are pending; no bench pair is assembled.
- Current radio adapter: RadioLib **LLCC68**, dependency pinned to **7.8.1**. RA-01SCH is not SX1262. A future SX1262 adapter is not implemented.
- Provisional bench profile: 915.0 MHz, BW125, SF7, CR4/5, explicit header, PHY CRC, preamble 8, private sync 0x12, 10 dBm and 1 MHz SPI. Control SF9 is reserved and unused in M2. SF10–SF12 must not be used at BW125 on LLCC68.
- Pins, USB path, supply transients, antenna matching and locally permitted RF settings remain unverified. Read [HARDWARE.md](docs/en/HARDWARE.md) before assembly or transmission.

## Architecture principles

One image loads a validated `GATEWAY` or `CLIENT` role from NVS. Missing, corrupt or unsupported configuration enters provisioning mode with application TX disabled. Role edits apply only after save and controlled reboot; identical hardware is not used to infer roles.

The implemented `RadioTransport` reference separates application state machines from `RadioDriver` state policy. Its non-owning `RadioHardware` reference delegates to the sole LLCC68/SPI/GPIO owner, `Llcc68Hardware`; native fault tests execute the actual driver. NvsConfigStore also runs natively through five-operation ConfigStorage; PreferencesConfigStorage solely owns real Preferences access. See the [storage audit](docs/en/CONFIG_STORE_AUDIT.md). Native readback does not prove flash durability. Esp32Firmware owns the two concrete adapters before the reference-injected FirmwareRuntime; deterministic native tests execute the actual coordinator, store, driver, apps and console. See the [runtime audit](docs/en/RUNTIME_AUDIT.md). SDK shims simulate clock/serial/reset/raw RNG; they do not validate hardware entropy or reset. Generation is bounded to eight raw calls; unavailable identity suppresses normal TX, logs SESSION_ERROR and retries at one-second intervals. See the [dependency budget audit](docs/en/DEPENDENCY_BUDGET_AUDIT.md) for corrected timestamp boundaries and remaining SDK/BUSY/allocation limits. The codec/config/sequence modules are host-testable. `main.cpp` is a 15-line entry point.

Four target boundaries are documented in [ARCHITECTURE.md](docs/en/ARCHITECTURE.md): protocol core, radio adapters, platform services and an application payload/event interface. Platform services are currently ESP32/Arduino-specific; a general application submission/delivery API is still absent. The protocol core contains no Modbus, meter-model, Wi-Fi/MQTT, PostgreSQL or dashboard integration. Do not treat the target architecture as fully implemented.

## Repository structure

```text
Adaptive-LoRa/
├── include/             # Provisional pins, fixed PHY and project limits
├── src/app/             # ESP32 runtime, NVS and bounded console
├── src/common/protocol/ # Frame codec and sequence/session helpers
├── src/common/radio/    # Transport contract and LLCC68 adapter
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
pio run -e esp32c3_supermini
pio test -e native
```

The firmware target is `esp32-c3-devkitm-1` with Arduino and RadioLib 7.8.1. The recorded workstation resolved to pioarduino/espressif32 55.03.311, Arduino-ESP32 3.3.11, RISC-V GCC 14.2.0, Core 6.1.19; native used GCC 6.3.0, native 1.2.1 and Unity 2.6.1. The platform is not pinned, so record the resolved packages rather than assuming a different installation is identical. The source manifest and command logs identify each validation snapshot.

## Provisioning and future bench procedure

Serial: 115200 baud. Commands: `help`, `status`, `config show`, `config set role gateway|client`, `config set network-id N`, `config set node-id N`, `config set gateway-id N`, `config save`, `reboot`. Optional `config set hello-interval-ms <1000..3600000>`; default 5000 ms after transaction completion.

Initial IDs (network/node/gateway) are gateway `1/1/1` and client `1/2/1`. Set all IDs explicitly; role edits preserve IDs. `config show` is staged state, `status` is loaded state. Network IDs are 1–65535; node/gateway IDs are 1–65534. See the deferred [bench runbook](docs/en/BENCH_RUNBOOK.md) and [run-record template](logs/test/RUN_RECORD_TEMPLATE.md); no upload or RF test is claimed here.

## Development roadmap

1. Finish software verification and resolve dependency timing/allocation assumptions within M0–M2.
2. After procurement/assembly, verify NVS/reboot, pins/supply/IRQ and the two-device HELLO/ACK link.
3. M3: one-client beacon/TDMA after measuring radio transitions and clock drift.
4. M4–M6: bitmap/selective retry, versioned conservative ADR and synchronization recovery, with fault tests before physical acceptance.
5. M7–M8: controlled reference-vs-Moxon evaluation and explicitly labelled logical-node simulations.
6. Add application payload/event and platform-service interfaces in a separate reviewed task before integrating other embedded applications.

The [milestone plan](docs/en/MILESTONE_TASKS.md) and [test plan](docs/en/TEST_PLAN.md) define acceptance gates. No later MAC implementation is included in the identity migration.

## Research and reproducibility

Preserve chronology, failed attempts, error resolutions, exact commands, package versions and the distinction between compile, host simulation and physical evidence. Historical records are sanitized publication copies: identity/path labels were redacted, not results reclassified. Old hashes identify original artifacts; see [IDENTITY_AUDIT.md](docs/en/IDENTITY_AUDIT.md) and [logs guidance](logs/README.md).

Use general application payloads; keep integrations outside protocol logic. Publish assumptions, controlled test conditions and limitations. Do not claim link capacity, antenna gain or hardware validation without evidence. Do not redistribute full research papers without permission. Third-party dependencies remain external; [THIRD_PARTY.md](THIRD_PARTY.md) records their licenses. No project-wide redistribution license has been selected in this task.

Read [AGENTS.md](AGENTS.md) before changes. English is canonical; [简体中文](README.zh-CN.md) is the secondary documentation language.
