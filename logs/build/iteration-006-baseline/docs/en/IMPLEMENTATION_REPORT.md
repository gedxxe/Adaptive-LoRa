# M0–M2 implementation report — 2026-11-12

Candidate `0.2.0-alpha.1`, protocol `1`: one firmware image, validated NVS role loading, bounded provisioning, LLCC68 fixed-PHY HELLO/ACK, explicit v1 codec, ACK echo checks and duplicate handling are implemented. Firmware compile and native tests pass. Physical M0–M2 acceptance is pending. No commits were created; the workspace has no Git repository.

## Executed validation

Both commands used the installed PlatformIO virtual environment outside the restricted sandbox:

```powershell
& <PIO_PYTHON> -m platformio run -e esp32c3_supermini
& <PIO_PYTHON> -m platformio test -e native
```

| Evidence | Result | Full output |
|---|---|---|
| Frozen-source ESP32 build | Exit 0, PASS | `logs/build/iteration-005-esp32.log` |
| Native tests | Exit 0, 44 passed / 0 failed / 0 skipped | `logs/test/iteration-005-native.log` |
| Native categories | Codec 18; console 10; role/config 9; sequence/timing 7 | Same native log |
| Source review | 29 C++/header/test files; one LLCC68 owner; no SX1262 object, Arduino String, explicit new or delay in project source | `logs/build/iteration-005-static-review.log` |
| Capture helper | Help exit 0; Python syntax exit 0 | `logs/test/iteration-004-capture-help.log`, `logs/test/iteration-004-capture-syntax.log` |
| Serial port inventory | Exit 0; six Bluetooth ports; no identified ESP32 port | `logs/test/iteration-004-serial-ports.log` |
| Upload / physical NVS / radio / two-device RF | NOT RUN | No physical logs or measurements |

`main.cpp` is explicitly confirmed thin: **15 lines**, only runtime construction and setup/loop delegation. Source edits and all added files were reviewed. `git diff --check` cannot produce a diff because this directory is not a Git repository; no Git metadata was initialized. The saved source manifest identifies the reviewed source/test/config snapshot.

Resolved toolchain: PlatformIO Core **6.1.19**; installed pioarduino/espressif32 **55.03.311** (display 55.3.311); Arduino-ESP32 **3.3.11**, libs **5.5.5+sha.b774170ff46**; RISC-V GCC **14.2.0** (`esp-14.2.0_20260121`); RadioLib **7.8.1** pinned and resolved; native platform **1.2.1**, Unity **2.6.1**, MinGW GCC **6.3.0**. Package/actual compiler outputs are in `logs/build/iteration-004-packages-final.log`, `logs/build/iteration-004-compiler-retry.log` and `logs/test/iteration-004-host-compiler.log`. The platform is not pinned by the starter contract; another workstation may resolve a different espressif32 distribution.

Built application image: `.pio/build/esp32c3_supermini/firmware.bin`; SHA-256 `d5cc157843dd895a51f0b26a20f0a3e0ecbe461a8886d2e457f1c423f0e878b7`. Reported RAM 24,516 / 327,680 bytes, flash 363,334 / 1,310,720 bytes. BUILD_ID is the candidate label `m0-m2-source-001`; use the binary hash and `logs/build/iteration-005-source-manifest.sha256` to identify this exact snapshot. No reproducible-binary claim is made.

## Every C++ translation unit

| File | Responsibility |
|---|---|
| `src/main.cpp` | Thin Arduino entry point; delegates to runtime |
| `src/app/firmware_runtime.cpp` | Loads immutable boot config, constructs only the selected app, dispatches console and controlled reboot |
| `src/app/nvs_config_store.cpp` | Preferences single-key blob load/save, validation and readback; no per-packet writes |
| `src/app/provisioning_console.cpp` | Pure bounded parser, staged edits, strict numeric handling and command actions |
| `src/common/config/device_config.cpp` | Pure role/ID/schema/interval validation and explicit 24-byte config serialization/integrity |
| `src/common/radio/radio_driver.cpp` | Sole LLCC68/SPI owner; fixed initialization, minimal ISR flag, asynchronous TX/RX, metrics and bounded TX deadline |
| `src/common/protocol/frame_codec.cpp` | Canonical 28-byte little-endian header, 96-byte bounds, frame validation and nine-byte ACK identity helpers |
| `src/common/protocol/sequence_tracker.cpp` | Eight bounded session windows and sequence exhaustion guard |
| `src/common/diagnostics/serial_logger.cpp` | Bounded queued key/value diagnostics and limited nonblocking serial drain |
| `src/gateway/gateway_app.cpp` | HELLO validation, duplicate/stale handling, ACK echo generation and receive recovery |
| `src/client/client_app.cpp` | HELLO interval/state, outstanding identity, matching ACK or timeout, session rotation and counters |
| `test/test_frame_codec/test_main.cpp` | 18 codec/layout/bounds/ACK tests |
| `test/test_role_config/test_main.cpp` | 9 role/ID/schema/blob tests |
| `test/test_sequence_tracker/test_main.cpp` | 7 identity/window/exhaustion/deadline tests |
| `test/test_provisioning_console/test_main.cpp` | 10 parser/staging/workflow/bounds tests |

## Configuration and physical limits

NVS model/blob corruption detection and parser workflows passed native tests. Actual Preferences read/write, persistence across power loss, corrupt/incompatible saved config boot behavior and role changes on physical hardware remain **NOT RUN**. A successful compile is not an NVS provisioning result.

All proposed pins remain provisional. Exact board/module revisions, flash/USB path, pad labels and orientation, continuity, regulator and TX rail measurements, antenna/feed/matching, module oscillator/regulator/RF-switch behavior and authorized RF settings remain unverified. RadioLib's native USB CDC sleep warning is retained (ERR-005); M0–M2 does not request MCU sleep. Chip diagnostics report selected driver/init result; the library does not expose its matched identification string.

M2 remains fixed SF7/BW125/CR4/5 at provisional 915 MHz and 10 dBm, explicit header, PHY CRC, preamble 8, private sync 0x12, SPI 1 MHz. No TDMA, bitmap, retransmission scheduler, ADR or excluded integration is implemented. The deduplication cache is bounded and volatile; evicted sessions/reboots lose history.

## Two-device serial procedure and status

Follow `docs/en/BENCH_RUNBOOK.md` (Chinese companion in `docs/zh-CN/`). Complete hardware/RF preconditions first. Flash the same image to both identified ports, provision gateway IDs `1/1/1` and client IDs `1/2/1` (network/node/gateway), save and reboot. Verify invalid config leaves TX disabled; verify physical NVS persistence and LLCC68/BUSY/DIO1/reset on both units. Use `tools/capture_bench.py` to save both raw serial streams to a new run directory and fill `logs/test/RUN_RECORD_TEMPLATE.md`. Record >=200 actual HELLO attempts, accepted ACKs, timeouts/TX failures, duplicates, RSSI/SNR, latency, resets/errors and log drops. The short-range target is >=99% accepted ACK transactions; measure rather than claim it.

M0, M1 and M2 are all **IN PROGRESS** pending those physical acceptance records. Next concrete task is M0 inspection and approved bench setup, followed by M1 provisioning/bring-up and the M2 two-device run. No physical test was performed during this task.

## M3–M8 implementation plan — not started

| Milestone | Implementation and acceptance gate |
|---|---|
| M3 | After M2 passes, define beacon payload, frame/schedule versions and one-client lease/slot state. Measure TX/RX switching, IRQ timing and drift; derive guard. Accept only assigned-slot TX and blocked normal uplink on missed/stale beacon, with paired timing logs. |
| M4 | Add schedule-indexed LSB-first bitmap and deterministic selective retry (initial limit two). Host-test 1/10/70 bits, unused bits and schedule mismatch; inject DATA/bitmap loss and duplicates. Preserve identity, distinguish MISSING/ACK_UNKNOWN and prove one application delivery. |
| M5 | Add experimental link windows, hysteresis and gateway-only SF7/SF8/SF9 decisions at BW125. Implement versioned propose/accept/commit at frame boundaries. Test lost proposal/accept/commit, stale config and reboot; retain previous committed client profile without valid commit. |
| M6 | Add synchronization lease expiry and bounded rejoin request/grant. Test missed beacons, both-device reboot and stale schedules; prove unsynchronized clients cannot transmit normal DATA. |
| M7 | Compare a real reference antenna and Moxon with the same radio pair/PHY/power/positions/heights/orientation/packet counts. Record PDR/ACK/RSSI/SNR/latency and environment. No unsupported gain claim. |
| M8 | Simulate 5/10/20/35/70 logical clients using measured switching/guard costs and stated packet/PHY/reporting/retry/join assumptions. Report airtime/utilization/feasibility as simulation; additional physical nodes require separate hardware testing. |

Detailed gates remain in `docs/en/TEST_PLAN.md` and `docs/en/MILESTONE_TASKS.md`. No later-milestone source was started.

## Durable records

- `docs/en/BUILD_MEMORY.md`: append-only iterations 001–005, including failed attempts and corrections.
- `docs/en/ERROR_LOG.md`: ERR-001–007; software compile/test issues closed, USB hardware verification open.
- `docs/en/PROGRESS.md`: current milestone/toolchain/evidence snapshot.
- `docs/en/DECISIONS.md`: ADR-009 implementation choices; Chinese companion added.
- `CHANGELOG.md`: implementation candidate and actual validation limits.

Exact created/modified file inventory, including logs, follows below. Generated `.pio/` SDK/build/cache content and ignored Python bytecode are not source changes; pre-existing `.gitkeep` placeholders are preserved.

## Exact changed-file inventory

19 existing files modified; 37 source/document/tool/template files created; 23 evidence files created.

Modified existing files:

```text
CHANGELOG.md
README.md
README.zh-CN.md
VERSION
docs/en/ARCHITECTURE.md
docs/en/BUILD_MEMORY.md
docs/en/DECISIONS.md
docs/en/ERROR_LOG.md
docs/en/HARDWARE.md
docs/en/LOGGING_GUIDE.md
docs/en/PROGRESS.md
docs/en/PROTOCOL.md
docs/en/TEST_PLAN.md
docs/zh-CN/ARCHITECTURE.md
docs/zh-CN/HARDWARE.md
docs/zh-CN/PROGRESS.md
docs/zh-CN/PROTOCOL.md
docs/zh-CN/TEST_PLAN.md
platformio.ini
```

Created source/document/tool/template files:

```text
.gitignore
docs/en/BENCH_RUNBOOK.md
docs/en/IMPLEMENTATION_REPORT.md
docs/zh-CN/BENCH_RUNBOOK.md
docs/zh-CN/DECISIONS.md
docs/zh-CN/LOGGING_GUIDE.md
include/board_pins.h
include/project_config.h
include/radio_config.h
logs/test/RUN_RECORD_TEMPLATE.md
src/app/firmware_runtime.cpp
src/app/firmware_runtime.h
src/app/nvs_config_store.cpp
src/app/nvs_config_store.h
src/app/provisioning_console.cpp
src/app/provisioning_console.h
src/client/client_app.cpp
src/client/client_app.h
src/common/config/device_config.cpp
src/common/config/device_config.h
src/common/diagnostics/serial_logger.cpp
src/common/diagnostics/serial_logger.h
src/common/protocol/frame_codec.cpp
src/common/protocol/frame_codec.h
src/common/protocol/message_types.h
src/common/protocol/sequence_tracker.cpp
src/common/protocol/sequence_tracker.h
src/common/radio/radio_driver.cpp
src/common/radio/radio_driver.h
src/gateway/gateway_app.cpp
src/gateway/gateway_app.h
src/main.cpp
test/test_frame_codec/test_main.cpp
test/test_provisioning_console/test_main.cpp
test/test_role_config/test_main.cpp
test/test_sequence_tracker/test_main.cpp
tools/capture_bench.py
```

Created evidence files (including failed attempts):

```text
logs/build/iteration-001-dependencies-retry.log
logs/build/iteration-001-dependencies.log
logs/build/iteration-001-esp32.log
logs/build/iteration-004-compiler-retry.log
logs/build/iteration-004-compiler.log
logs/build/iteration-004-esp32.log
logs/build/iteration-004-packages-final.log
logs/build/iteration-004-packages-utf8.json
logs/build/iteration-004-packages.log
logs/build/iteration-004-source-manifest.sha256
logs/build/iteration-004-static-review.log
logs/build/iteration-005-esp32.log
logs/build/iteration-005-file-inventory.txt
logs/build/iteration-005-source-manifest.sha256
logs/build/iteration-005-static-review.log
logs/test/iteration-001-native.log
logs/test/iteration-002-native.log
logs/test/iteration-003-native.log
logs/test/iteration-004-capture-help.log
logs/test/iteration-004-capture-syntax.log
logs/test/iteration-004-host-compiler.log
logs/test/iteration-004-serial-ports.log
logs/test/iteration-005-native.log
```
