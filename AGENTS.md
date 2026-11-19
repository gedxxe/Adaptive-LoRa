# AGENTS.md — Adaptive-LoRa engineering contract

## Project and current baseline

Adaptive-LoRa is an independent custom LoRa P2P research project, not LoRaWAN.
Research title: Reliability-aware TDMA Scheduling and Link Adaptation for
Single-Radio LoRa Industrial Energy Monitoring Networks. Energy monitoring is
an application domain, not a protocol dependency. Keep customer identities,
private systems and machine-specific paths out of publication content.

The bench pair is **Heltec WiFi LoRa 32 V3.4**, ESP32-S3/SX1262, with two
stock monopoles and a user-confirmed 902–928 MHz procurement variant. The
owner confirms V3.2 wiring and pin mapping and reports SMT and crystal
improvements. Use the V3.2 schematic and Arduino V3 variant as references.
Verify each board's physical SKU, antenna and peripheral behavior at the
bench. Do not assume PSRAM.
Build environment: `heltec_wifi_lora_32_V3`, Arduino/C++17, with
`jgromes/RadioLib@7.8.1`. Firmware `0.2.0-alpha.1`, protocol `1`, NVS schema `1`.
Authoritative versions/settings are VERSION, PROTOCOL_VERSION,
include/project_config.h and platformio.ini; resolved package versions and
actual test results belong in [PROGRESS.md](docs/en/PROGRESS.md).

The fixed-PHY M0–M2 HELLO/ACK firmware exists and has host/build evidence.
**M0–M2 physical acceptance is IN PROGRESS.** Confirm the physical SKU,
electrical behavior, flash/NVS, supply and timing at the bench. M3–M8
production features are NOT STARTED. The Iteration 015 scheduling work is
host research. Historical records retain their original hardware and results.

## Read before editing

Inspect the workspace, Git status and fetched remote refs before changing files.
Preserve user work. Read README.md and these canonical English documents:

- [ARCHITECTURE.md](docs/en/ARCHITECTURE.md), [HARDWARE.md](docs/en/HARDWARE.md),
  [PROTOCOL.md](docs/en/PROTOCOL.md), [TEST_PLAN.md](docs/en/TEST_PLAN.md).
- [PROGRESS.md](docs/en/PROGRESS.md), [BUILD_MEMORY.md](docs/en/BUILD_MEMORY.md),
  [ERROR_LOG.md](docs/en/ERROR_LOG.md), [DECISIONS.md](docs/en/DECISIONS.md),
  [VERSIONING.md](docs/en/VERSIONING.md).

Read source and tests for the affected boundary. For hardware work also read
[HARDWARE_MIGRATION_AUDIT.md](docs/en/HARDWARE_MIGRATION_AUDIT.md) and
[BENCH_RUNBOOK.md](docs/en/BENCH_RUNBOOK.md). Future scheduling work starts with
[MILESTONE_TASKS.md](docs/en/MILESTONE_TASKS.md) and
[TDMA_FEASIBILITY.md](docs/en/TDMA_FEASIBILITY.md).
English is canonical; align the Chinese companion when a decision or operator
procedure changes. CODEX_START_PROMPT.md is an archived contract, not current
instructions. Do not update old logs or historical results to match this target.

## Hardware and operating constraints

Pins and PHY belong in include/board_pins.h and include/radio_config.h.
The V3 reference uses GPIO 8/9/10/11/12/13/14 for NSS/SCK/MOSI/MISO/RESET/BUSY/DIO1.
DIO2 controls the RF switch; DIO3 supplies the 1.8 V TCXO. Pass oscillator and
DC-DC selection explicitly. The OLED candidate uses the Arduino variant’s OLED and Vext aliases in the
opt-in `heltec_wifi_lora_32_V3_oled` build. Verify GPIO17/18/21/36, Vext
polarity and SSD1306/0x3C before using it on the delivered boards. Preserve
other pin reservations in HARDWARE.md.

Retain 915.0 MHz provisional, BW125, data SF7, reserved control SF9, CR4/5,
preamble 8, explicit header, CRC, sync 0x12, 10 dBm and 1 MHz SPI. Future ADR
research is limited to SF7–SF9/BW125; no automatic bandwidth/power changes.
One radio time-shares control and data; it cannot receive multiple SFs at once.

Before power, identify the board, RF variant and antenna connector and connect a suitable
50-ohm antenna or load, including when the factory image is unknown. Use a
stable regulated 5 V supply with at least 500 mA input capability; measure
actual supply margin/current and check battery polarity. Do not parallel
regulators or back-power through GPIO. Moxon fabrication, matching and gain
are unmeasured. Radiated testing requires an authorized RF setup.

## Ownership and compatibility

Use one firmware image. Valid NVS role selects GatewayApp or ClientApp;
missing/corrupt/incompatible config enters provisioning with application TX
disabled. Never infer role from identical hardware. Config edits are staged,
validated, saved with readback, then activated only after controlled reboot.
Gateway node ID equals gateway ID; client node ID differs. Network is
1–65535; node/gateway IDs are 1–65534. Defaults are network 1, gateway 1,
client 2. Keep the bounded serial commands and status semantics in README
and ARCHITECTURE; do not write per-packet telemetry to NVS.

| Owner | Responsibility |
|---|---|
| src/main.cpp | Thin Arduino setup/loop delegate; approximately 15–40 lines, no protocol/radio/state policy |
| Esp32Firmware | Own concrete storage/radio adapters before and beyond the coordinator lifetime |
| FirmwareRuntime | Load configuration, initialize radio, select role and dispatch bounded loop work |
| GatewayApp / ClientApp | Actual role state machines through RadioTransport |
| RadioDriver | Sole radio state/deadline/error policy through RadioHardware |
| Sx1262Hardware | Sole Module/SX1262/SPI/GPIO owner; ISR only sets a minimal flag |
| NvsConfigStore | Load/save/verify policy through non-owning ConfigStorage |
| PreferencesConfigStorage | Sole Preferences/NVS adapter |
| DeviceConfig / FrameCodec / SequenceTracker | Pure validation, explicit serialization and bounded session history |
| OledStatusDisplay | Optional SSD1306 diagnostics using runtime snapshots after radio service |
| ProvisioningConsole / SerialLogger | Bounded command parsing / queued diagnostics; no radio ownership |

Test actual production modules through existing low-level seams; never copy
state machines into fakes. Keep protocol logic free of MCU/radio dependencies
where already pure and free of meter, fieldbus, backhaul, database and UI
coupling. RadioTransport exists; complete clock/random/logging portability
and a general application payload/event API do not. Record these gaps rather
than claiming all target boundaries are implemented.

Keep fixed buffers, explicit little-endian serialization, 28-byte header,
96-byte frame limit and nine-byte ACK payload. Preserve protocol v1 HELLO/ACK,
error classifications and NVS namespace/key/schema/blob compatibility.
ACK must match the outstanding client source/boot/sequence and deadline.
Retries in future milestones reuse identity and payload. The eight-session,
64-sequence volatile cache is bounded M2 behavior, not a 70-client or reboot
replay guarantee. Do not enlarge/change it without milestone justification.

Use monotonic, wrap-safe deadlines within the half-range of their timer type.
Keep application hot paths allocation-free and avoid Arduino String,
blocking delays, unbounded parsing and ISR SPI/logging/allocation. Check
return codes and lengths. Dependency calls may still block: ERR-021 BUSY
launch and ERR-023 pre-runtime NVS formatting remain open. A driver deadline
or unspecified watchdog does not prove bounded recovery. Do not patch
third-party libraries or add abstractions speculatively.

## Scope and next work

Next: enumerate one terminated board at a time in Windows, make verified
same-board private backups, and follow the bench runbook. Set
`config set hello-interval-ms 15000` on both roles for the smoke test and
OLED freshness.
Measure transitions, IRQ latency, processing and drift before choosing M3
guards. Host-only feasibility tooling is preliminary research; production
BEACON/TDMA, bitmap, selective retry and ADR require separate milestone work.
Do not create scheduler/ADR directories merely for planning.

Do not add Wi-Fi/backhaul, Modbus/RS485, databases/dashboards, LoRaWAN/SX1302,
OTA, multi-SF concurrent reception, battery features or unrelated
libraries. Keep one radio adapter until another has a concrete reviewed need.
Avoid factories, dependency-injection frameworks and unrelated refactoring.

## Verification and engineering records

Before reporting a coding iteration complete, run and check exit codes:

```sh
pio run -e heltec_wifi_lora_32_V3
pio run -e heltec_wifi_lora_32_V3_oled
pio test -e native
python -m unittest discover -s tools -p test_capture_bench.py -v
python -m unittest discover -s tools -p test_tdma_feasibility.py -v
python -m unittest discover -s tools -p test_check_consistency.py -v
python tools/check_consistency.py
```

Use installed PlatformIO Python for host tools. Inspect the complete diff
and thin main, then review every staged diff and `git diff --cached --check`.
Report exact commands, cases/counts, skipped cases, versions and limitations.
A compile, native test, analytical model and physical measurement are distinct
forms of evidence. Never invent commands/results or call simulation RF proof.
Unknown physical quantities are NOT MEASURED / NOT AVAILABLE / TBD.

Every coding/build iteration, including failure, must append BUILD_MEMORY,
refresh PROGRESS, record errors/blocking warnings in ERROR_LOG, append an ADR
when a decision changes, and update CHANGELOG for release/behavior changes.
Align Chinese decisions/procedures. Save useful complete command output under
logs/build or logs/test with exact references and disclosed path sanitization.
Record date, objective, branch/base, versions, file responsibilities, commands,
results, toolchain, role inputs, setup/metrics or explicit unknowns, findings
and next action. Preserve earlier iteration/error/ADR records and completed
logs byte-for-byte; append correction notes instead of rewriting evidence.
Publication identity/path redaction needs explicit authorization and retained
private originals; it must preserve chronology and outcomes.

## Git and publication

`main` is canonical. Start a scoped branch from fetched current origin/main;
use codex/ by default unless the task names a branch. Preserve historical
branches and records. Commit/push/create PR only when authorized by the user.
Integrate reviewed work through normal merge commits. Never force-push,
rebase, squash or delete historical branches during synchronization.
PR #5 merged Iteration 015 at `e34e7296135b47ecd3af479fe0593f4118bc734b`.
Iteration 016 authorizes the branch `iteration-016-v34-physical-validation`
and a PR for review. Keep main unchanged until review.

Publish sanitized project files only. Exclude caches, binaries, credentials,
local configuration, private backups and unlicensed research PDFs. Preserve
third-party license references. Final report lists changed files/ownership,
actual build/test evidence, behavior/compatibility, unresolved risks, physical
checks performed, milestone state, next task and publication state.
