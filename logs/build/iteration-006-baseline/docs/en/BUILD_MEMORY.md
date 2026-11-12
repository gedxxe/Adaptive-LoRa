# Build Memory — Append Only

This file is the durable engineering memory for every coding/build
iteration. Add a new section at the end after each iteration, including
failed builds and partially completed work. Never replace an earlier
entry.

## Rules

- One iteration entry per completed coding/build attempt.
- A failed compile, failed test, or blocked hardware attempt still gets
  an entry.
- Record exact commands and exit codes.
- Do not invent measurements, logs, tool versions, or test outcomes.
- Use `NOT MEASURED`, `NOT AVAILABLE`, or `TBD` where appropriate.
- Store complete terminal output under `logs/build/` or `logs/test/` and
  reference it here when available.
- If a previous entry needs correction, append a correction note
  referencing its iteration ID.
- Update `PROGRESS.md` in the same iteration.
- Add errors to `ERROR_LOG.md`; add architectural changes to
  `DECISIONS.md`.

## Required iteration template

Copy this section for each new iteration:

``` markdown
## ITERATION-XXX — YYYY-MM-DD HH:MM WIB

### Objective
- Milestone:

- Intended outcome:

### Source baseline
- Firmware version:

- Protocol version:

- Git branch:

- Git commit before:

- Git commit after:

- Working tree state:

### Hardware and configuration
- Board revision:

- Radio module marking/revision:

- Device role:

- Network ID / node ID / gateway ID:

- Antenna and orientation:

- Distance / LOS-NLOS:

- Supply voltage:

- PHY / TX power:

### Files changed
| File | Change | Reason |
|---|---|---|

### Toolchain
- PlatformIO Core:

- Platform:

- Arduino core:

- Compiler:

- RadioLib:

- Host compiler/test framework:

### Commands and results
| Command | Exit code | Result | Full log path |
|---|---:|---|---|

### Unit tests
- Tests executed:

- Passed:

- Failed:

- Skipped:

- Failure details:

### Physical test results
- Packets attempted:

- Packets received:

- ACK success:

- PDR:

- RSSI:

- SNR:

- Retries:

- Latency:

- Resets / driver errors:

- Raw log path:

### Errors and diagnosis
- Error IDs:

- Observed behavior:

- Root cause:

- Fix attempted:

- Fix verification:

### Known gaps / unverified assumptions
-

### Next iteration
- Exact next task:

- Acceptance criteria:
```

------------------------------------------------------------------------

## ITERATION-000 — Documentation baseline

### Objective

Define the initial firmware architecture, hardware constraints, protocol
outline, milestone sequence, and documentation-memory rules.

### Source baseline

- Firmware version baseline: `0.1.0-alpha.1` (planning baseline only)
- Protocol version baseline: `1` (wire-format specification baseline
  only)
- Git branch/commit: NOT AVAILABLE
- Firmware source implementation: NOT INCLUDED IN THIS DOCUMENTATION
  PACKAGE

### Decisions established

- One firmware image for both physically identical ESP32-C3 devices.
- Role is loaded from NVS at boot; invalid/unset role enters
  provisioning mode with TX disabled.
- Radio module is Ai-Thinker Ra-01SCH / LLCC68; use RadioLib `LLCC68`.
- Initial PHY is fixed; first implementation is M0–M2 HELLO/ACK.
- `main.cpp` must remain a thin entry point.
- Every future build iteration must append a record here.

### Commands and results

- Firmware build: NOT RUN.
- Native unit tests: NOT RUN.
- Physical RF tests: NOT RUN.

### Known gaps

- Board revisions, pin mapping, power rail, antenna matching, regulatory
  channel, and toolchain versions remain unverified.
- No firmware source code is claimed to exist or compile from this
  documentation baseline.

### Next iteration

Implement M0–M2 and replace all TBD fields with measured or
tool-reported evidence.

## ITERATION-001 — 2026-11-12 WIB — initial M0–M2 implementation

- Author: Codex. Objective: implement M0–M2 software, preserve the canonical v1 wire layout.
- Firmware: `0.2.0-alpha.1` implementation candidate; protocol: `1`.
- Git branch / before commit / after commit: NOT AVAILABLE (workspace is not a Git repository). No commit created.
- Existing baseline: populated documentation, starter platformio.ini, empty source/include/test directories.
- Hardware: two units specified as ESP32-C3 Super Mini + Ra-01SCH/LLCC68; actual board/module revisions, labels, continuity and USB path NOT AVAILABLE. No physical unit accessed.
- Roles planned: GATEWAY network/node/gateway `1/1/1`; CLIENT `1/2/1`. No physical provisioning performed.
- Antenna: 915 MHz Moxon per inventory; orientation, distance, LOS/NLOS, supply voltage, packet counts, PDR, ACK success, RSSI, SNR, retries, latency, resets and radio errors: NOT MEASURED. Physical RF test: NOT RUN.
- Configured PHY: fixed SF7/BW125/CR4/5, 915.0 MHz provisional, 10 dBm, explicit header, PHY CRC, preamble 8, sync word 0x12, SPI 1 MHz. XTAL and LDO assumptions require hardware verification.

### Files changed

- `.gitignore`: exclude generated PlatformIO files.
- `platformio.ini`: native pure-module filters, source includes, warnings, native USB CDC bench assumption.
- `VERSION`, `include/project_config.h`: candidate version and centralized limits/deadlines.
- `include/board_pins.h`, `include/radio_config.h`: provisional pins and fixed PHY.
- `src/main.cpp`: 15-line Arduino delegation only.
- `src/app/firmware_runtime.h/.cpp`: immutable loaded config, role construction, serial dispatch and controlled reboot.
- `src/app/nvs_config_store.h/.cpp`: validated single-blob Preferences persistence and readback.
- `src/app/provisioning_console.h/.cpp`: bounded staged command parser.
- `src/common/config/device_config.h/.cpp`: pure config validation and 24-byte storage representation with integrity check.
- `src/common/protocol/message_types.h`: reserved type allocation only.
- `src/common/protocol/frame_codec.h/.cpp`: explicit little-endian v1 framing and ACK identity checks.
- `src/common/protocol/sequence_tracker.h/.cpp`: bounded eight-session, 64-sequence duplicate window and exhaustion guard.
- `src/common/radio/radio_driver.h/.cpp`: sole LLCC68/SPI owner, asynchronous IRQ-driven TX/RX and bounded TX timeout.
- `src/common/diagnostics/serial_logger.h/.cpp`: bounded queued key/value serial output.
- `src/gateway/gateway_app.h/.cpp`: HELLO validation and idempotent re-ACK.
- `src/client/client_app.h/.cpp`: HELLO state machine, ACK identity and timeout.
- `test/test_frame_codec/test_main.cpp`, `test/test_role_config/test_main.cpp`, `test/test_sequence_tracker/test_main.cpp`, `test/test_provisioning_console/test_main.cpp`: native acceptance cases.
- `docs/en/BUILD_MEMORY.md`, `docs/en/PROGRESS.md`, `docs/en/ERROR_LOG.md`, `docs/en/DECISIONS.md`, `CHANGELOG.md`, `docs/zh-CN/PROGRESS.md`: iteration evidence, current state and decisions.

### Commands and evidence

Commands run from workspace root. PlatformIO invoked as `& <PIO_PYTHON> -m platformio` (abbreviated PIO below); full stdout/stderr redirected to named logs.

| Command | Exit | Result | Log |
|---|---:|---|---|
| `python -m platformio --version` | 1 | Default Python lacks module; located installed virtual environment | tool discovery output |
| `PIO --version` | 0 | Core 6.1.19 | tool discovery output |
| `PIO pkg install -e esp32c3_supermini` | 1 | Sandbox denied platforms.lock | `logs/build/iteration-001-dependencies.log` |
| `PIO pkg install -e esp32c3_supermini` outside sandbox | 0 | RadioLib 7.8.1 installed | `logs/build/iteration-001-dependencies-retry.log` |
| `g++ --version` inside sandbox | process launch failed | Illegal System DLL Relocation; no compiler executed | tool discovery output |
| `g++ --version` outside sandbox | 0 | MinGW GCC 6.3.0 | tool discovery output |
| `PIO run -e esp32c3_supermini` outside sandbox | PENDING | SDK dependency setup still running at native failure; completion will be appended | `logs/build/iteration-001-esp32.log` |
| `PIO test -e native` outside sandbox | PENDING | Native compilation errors observed; final exit/count will be appended | `logs/test/iteration-001-native.log` |

- ESP32 packages reported during setup: installed espressif32 is pioarduino fork 55.03.311 (display 55.3.311), Arduino-ESP32 3.3.11, framework libs 5.5.5+sha.b774170ff46, RISC-V toolchain package 14.2.0+20260121. These are setup observations, not successful build evidence.
- Host framework: PlatformIO native/Unity; resolved version will be recorded after execution.
- Native tests: intended 44 cases in four suites; no pass count claimed. Missing `<initializer_list>` in tests and `<cstdio>` in console test prevent compilation (ERR-002).
- Errors: ERR-001 environment restrictions; ERR-002 missing test includes. Firmware compile outcome pending.
- Next action: add explicit standard-library test includes, rerun native tests; finish ESP32 build and review source/diff. M0–M2 remain IN PROGRESS; no RF acceptance claimed.

### ITERATION-001 completion note — 2026-11-12 13:23 WIB

- Native command final exit code: 1. All four suites failed to compile, zero assertions executed. PlatformIO's "4 test cases" refers to errored suites here.
- ESP32 command remains in dependency installation; no compile result claimed yet.

## ITERATION-002 — 2026-11-12 13:25 WIB — explicit native includes

- Author: Codex; milestone M0–M2; objective: fix ERR-002 and execute native tests.
- Source: firmware candidate 0.2.0-alpha.1 / protocol 1; Git branch/commits NOT AVAILABLE; no commits made.
- Files: four `test/test_*/test_main.cpp` files gained `<initializer_list>`; console test also gained `<cstdio>`. Updated this memory, PROGRESS, ERROR_LOG and Chinese progress snapshot. No production behavior changed.
- Command: `& <PIO_PYTHON> -m platformio test -e native` outside sandbox, stdout/stderr to `logs/test/iteration-002-native.log`; exit 1.
- Result: 44 named Unity cases executed, 43 passed and 1 failed (`test_payload_minimum_and_maximum`). PlatformIO summarizes 45 cases because its CTRL_BREAK_EVENT runner error adds a synthetic case after failure; this is not a 45th source test.
- Root cause: Unity's array assertion rejects count 0. Skip content comparison only for empty payload; retain length and round-trip assertions. ERR-002 fixed; ERR-003 opened.
- Toolchain: Core 6.1.19, Unity 2.6.1, MinGW GCC 6.3.0 (C++17 flag accepted). Native platform version pending package record. ESP32 setup versions unchanged from iteration 001; build still installing framework libs at native test completion.
- Hardware/config: no hardware accessed; planned gateway IDs 1/1/1 and client 1/2/1. Board/module revision, antenna orientation, distance/LOS, supply: NOT AVAILABLE / NOT MEASURED. Configured SF7/BW125/CR4/5, 915 MHz provisional, 10 dBm.
- Physical attempts, PDR, ACK success, RSSI/SNR, retries, latency and resets: NOT MEASURED. Native tests do not test physical NVS/Preferences or RF.
- Next: fix zero-length test assertion; review driver timeout/counters and module coupling; rerun native suites and finish firmware build. No milestone PASS.

### ITERATION-001 firmware command completion — 2026-11-12 WIB

- `PIO run -e esp32c3_supermini` finished with exit 1 after 619.62 s; full output `logs/build/iteration-001-esp32.log`.
- It found `GatewayApp` undeclared while source headers were being cleaned up in iteration 003. The build overlapped a multi-file edit: ClientApp's transitive GatewayApp include had been removed before Runtime's explicit include was visible to this compilation. ERR-004 records the failure; current source now explicitly includes both applications. Subsequent firmware builds must use a frozen source snapshot.
- RadioLib also warns about native USB CDC and sleep (ERR-005). This is not hidden or treated as physical serial evidence.

## ITERATION-003 — 2026-11-12 WIB — native pass and source review

- Author Codex; M0–M2; firmware candidate 0.2.0-alpha.1 / protocol 1; Git branch/commits NOT AVAILABLE. No commit created.
- Files changed: `test/test_frame_codec/test_main.cpp` guards zero-length content assertion; `src/common/protocol/sequence_tracker.h` owns shared BootIdGenerator alias; `src/gateway/gateway_app.h`, `src/client/client_app.h`, `src/app/firmware_runtime.h` remove client-to-gateway header dependency and add explicit runtime includes; `src/client/client_app.cpp` separates TX failures from ACK timeouts and reports transaction duration; `src/app/firmware_runtime.cpp` labels failed-load values as inactive defaults; `src/common/radio/radio_driver.cpp` enforces TX deadline even with stuck-high DIO1; `src/common/diagnostics/serial_logger.h/.cpp` add network/version fields and explicit N/A airtime, 512-byte bounded lines with truncation accounting. Memory/progress/error records updated.
- Command: `& <PIO_PYTHON> -m platformio test -e native` outside sandbox; exit 0; `logs/test/iteration-003-native.log`.
- Native: 44 passed / 0 failed / 0 skipped; codec 18, console 10, role/config 9, sequence/timing 7. All requested initial pure-module categories exercised; no Preferences or physical NVS test claimed.
- Toolchain: Core 6.1.19; Unity 2.6.1; native GCC 6.3.0; firmware package observations as iteration 001. Final firmware success not yet recorded.
- Hardware roles planned gateway 1/1/1, client 1/2/1; actual provisioned roles/revisions NOT AVAILABLE. PHY configured SF7/BW125/CR4/5, 915 MHz provisional, 10 dBm. Antenna orientation, LOS/distance, supply and all physical packet/ACK/PDR/RSSI/SNR/retry/latency/reset measurements: NOT MEASURED; physical tests NOT RUN.
- ERR-003 fixed and verified. ERR-004 firmware include failure occurred during overlapping edits and needs frozen-source build verification. ERR-005 USB warning requires bench serial check; firmware does not request MCU sleep.
- Next: finish operating documents and repeatable capture instructions, freeze source and build ESP32; record exact resolved versions, complete file inventory and static review. M0–M2 remain IN PROGRESS pending hardware acceptance.

## ITERATION-004 — 2026-11-12 13:43 WIB — frozen-source firmware validation

### Objective and baseline

- Author Codex; milestone M0–M2; finish ESP32 validation and operating records.
- Firmware `0.2.0-alpha.1`, protocol `1`, BUILD_ID `m0-m2-source-001`. Git branch/commit before/after NOT AVAILABLE; no commits.
- Source is the reviewed iteration-003 snapshot. No firmware source edits occurred during this build.
- Files added: `tools/capture_bench.py`; `logs/test/RUN_RECORD_TEMPLATE.md`; `docs/en/BENCH_RUNBOOK.md`, `docs/en/IMPLEMENTATION_REPORT.md`; `docs/zh-CN/BENCH_RUNBOOK.md`, `docs/zh-CN/DECISIONS.md`, `docs/zh-CN/LOGGING_GUIDE.md`.
- Existing files updated: `README.md`, `README.zh-CN.md`, `CHANGELOG.md`; English ARCHITECTURE/HARDWARE/PROTOCOL/TEST_PLAN/PROGRESS/BUILD_MEMORY/ERROR_LOG/DECISIONS/LOGGING_GUIDE and Chinese ARCHITECTURE/HARDWARE/PROTOCOL/TEST_PLAN/PROGRESS. Responsibilities: document actual behavior, provisional hardware, NVS representation, operating/capture procedure and evidence. Exact final inventory is in IMPLEMENTATION_REPORT.md.

### Commands and results

PIO means the exact invocation `& <PIO_PYTHON> -m platformio` from the workspace root; commands below redirected stdout/stderr to the named file.

| Command | Exit | Result | Log |
|---|---:|---|---|
| `PIO run -e esp32c3_supermini` outside sandbox | 0 | PASS; application/factory images generated; 45.92 s | `logs/build/iteration-004-esp32.log` |
| `PIO pkg list -e esp32c3_supermini` inside sandbox | 1 | cp1252 UnicodeEncodeError; metadata probe only | `logs/build/iteration-004-packages.log` |
| RISC-V compiler `--version` inside sandbox | launch failed | PowerShell StandardOutputEncoding startup error; no compiler exit code | `logs/build/iteration-004-compiler.log` (empty); tool transcript |
| `PIO pkg list -e esp32c3_supermini --json-output` with PYTHONUTF8=1 outside sandbox | 2 | Option unsupported by Core 6.1.19 | `logs/build/iteration-004-packages-utf8.json` (failure text, not JSON) |
| `PIO pkg list -e esp32c3_supermini` with PYTHONUTF8=1 outside sandbox | 0 | Resolved packages saved | `logs/build/iteration-004-packages-final.log` |
| `& <RISCV_GXX> --version` outside sandbox | 0 | GCC 14.2.0 / esp-14.2.0_20260121 | `logs/build/iteration-004-compiler-retry.log` |
| `g++ --version` outside sandbox | 0 | MinGW GCC 6.3.0 | `logs/test/iteration-004-host-compiler.log` |
| `& <PIO_PYTHON> tools/capture_bench.py --help` | 0 | CLI imports/help only; no serial ports opened | `logs/test/iteration-004-capture-help.log` |
| `python -m py_compile tools/capture_bench.py` | 0 | Syntax only | `logs/test/iteration-004-capture-syntax.log` |
| `& <PIO_PYTHON> -m serial.tools.list_ports -v` | 0 | Six Bluetooth ports; no identified ESP32 | `logs/test/iteration-004-serial-ports.log` |
| `git diff --check` | tool reported 1 | Not a Git repository; no diff available | tool transcript |
| Python inline static/source-manifest audit | 0 | 29 C++/header/test files, 15-line main, single LLCC68, no forbidden source constructs/conflicts/trailing whitespace | `logs/build/iteration-004-static-review.log` |

### Toolchain and tests

- Core 6.1.19; installed pioarduino/espressif32 55.03.311 (display 55.3.311); Arduino-ESP32 3.3.11 / libs 5.5.5+sha.b774170ff46; RISC-V GCC 14.2.0 package 14.2.0+20260121; RadioLib 7.8.1; native 1.2.1 / Unity 2.6.1 / MinGW GCC 6.3.0.
- Relevant native evidence unchanged: `PIO test -e native`, exit 0, iteration-003 log, 44 passed / 0 failed / 0 skipped: codec 18, parser 10, config 9, sequence/timing 7. No native test repeated for documentation/capture additions; helper checked via help and syntax.
- Firmware RAM 24,516/327,680; flash 363,314/1,310,720. Application image SHA-256 `bddc5e79a6865bd4c8cc3983adb595dfe1b21a332d6b5792d98d27d9e7d6e393`; source manifest `logs/build/iteration-004-source-manifest.sha256`.
- ERR-004 closed by frozen-source successful compile. ERR-006 metadata/startup probes resolved. RadioLib CDC warning retained, ERR-005 physical check open. The build also reports that a UTF-8 terminal is needed for its optional metrics display; standard RAM/flash size checks completed.

### Hardware, acceptance and next step

- Planned GATEWAY network/node/gateway 1/1/1; CLIENT 1/2/1. Actual saved roles, board/module revisions, pin continuity, USB routing and flash: NOT AVAILABLE. No device flashed or serial port opened for this task.
- Configured fixed SF7/BW125/CR4/5, 915 MHz provisional, 10 dBm; common control SF9 unused. XTAL/LDO/DIO2 assumptions require physical check.
- Supply, distance/LOS, antenna orientation/matching and RF authorization: NOT MEASURED / TBD. Physical packets/ACK success/PDR/RSSI/SNR/retries/latency/resets/driver behavior: NOT MEASURED; physical log path NOT AVAILABLE.
- Physical NVS persistence and invalid/corrupt/incompatible boot: NOT RUN. Host validation/representation is separate evidence.
- M0/M1/M2 IN PROGRESS; M3–M8 NOT STARTED. Next: finish M0 physical checks and permitted bench setup; then M1 provisioning/bring-up and >=200-attempt M2 run. A final driver-source review also identified that header-error filtering should use RadioLib's HEADER_VALID qualification; correct it before final delivery and rebuild.

### Timing clarification

Earlier checkpoint headings without a time recorded only the local date, not a measured wall-clock timestamp. Iteration 001's first captured native-completion checkpoint was 13:23 WIB; iteration 002 checkpoint was 13:25 WIB; iteration 003 native pass was observed before the 13:31 WIB toolchain checkpoint. Exact start/end times for those coding intervals are NOT AVAILABLE. No RF timing is inferred from these development timestamps.

## ITERATION-005 — 2026-11-12 13:46 WIB — final driver correction and validation

### Objective / baseline / files

- Author Codex; M0–M2 software candidate; firmware 0.2.0-alpha.1 / protocol 1 / BUILD_ID m0-m2-source-001. Git branch and before/after commits NOT AVAILABLE; no commit created.
- `src/common/radio/radio_driver.cpp`: qualify HEADER_ERR with absence of HEADER_VALID, matching pinned RadioLib readData; CRC_ERR remains unconditional. Ignore nonterminal IRQs. No wire-format or PHY change.
- `docs/en/BUILD_MEMORY.md`, `docs/en/ERROR_LOG.md`, `docs/en/PROGRESS.md`, `docs/en/TEST_PLAN.md`, `docs/en/IMPLEMENTATION_REPORT.md`, `docs/zh-CN/PROGRESS.md`, `docs/zh-CN/TEST_PLAN.md`, `CHANGELOG.md`: final results, review issue and evidence references. Final exact file list, including all earlier changes, is in IMPLEMENTATION_REPORT.md and `logs/build/iteration-005-file-inventory.txt`.
- Correction to iteration-004 file list: English DECISIONS.md's ADR-009 was actually appended in iteration 001, not newly changed in iteration 004. Final inventory correctly lists it as modified during the overall task.

### Commands / toolchain / unit tests

| Exact command (workspace root) | Exit | Result | Full log |
|---|---:|---|---|
| `$env:PYTHONUTF8 = '1'; & <PIO_PYTHON> -m platformio run -e esp32c3_supermini` outside sandbox | 0 | PASS, 15.09 s; only known RadioLib CDC warning | `logs/build/iteration-005-esp32.log` |
| `& <PIO_PYTHON> -m platformio test -e native` outside sandbox | 0 | 44/44 PASS, 7.01 s | `logs/test/iteration-005-native.log` |
| Python inline final source/static/hash audit | 0 | 29 files reviewed, thin 15-line main, sole LLCC68, no forbidden source constructs/conflicts/trailing whitespace; static IRQ-filter alignment | `logs/build/iteration-005-static-review.log` |

- Cases: codec 18, provisioning console 10, role/config 9, sequence/timing 7; 44 passed / 0 failed / 0 skipped. Names and individual outcomes are in the full native log. The radio hardware and Preferences are not native test doubles and are not claimed tested by these suites.
- Resolved versions unchanged and recorded from actual output: Core 6.1.19, pioarduino/espressif32 55.03.311, Arduino-ESP32 3.3.11 / libs 5.5.5+sha.b774170ff46, RISC-V GCC 14.2.0 (esp-14.2.0_20260121), RadioLib 7.8.1; native 1.2.1, Unity 2.6.1, MinGW GCC 6.3.0.
- RAM 24,516/327,680; flash 363,334/1,310,720. Final application SHA-256: `d5cc157843dd895a51f0b26a20f0a3e0ecbe461a8886d2e457f1c423f0e878b7`. Final source/test/config manifest: `logs/build/iteration-005-source-manifest.sha256`. Older hashes remain historical, not final.
- Source stayed frozen throughout final commands. ERR-007 source correction is statically aligned with pinned library and compiles; no physical error-rate verification claimed. ERR-001/002/003/004/006 closed. ERR-005 USB hardware verification remains open.

### Hardware and results

- Planned gateway role/IDs GATEWAY 1/1/1; client CLIENT 1/2/1. Actual saved roles, board/module markings/revisions, flash and USB route: NOT AVAILABLE.
- Proposed pins unchanged and provisional. PHY fixed SF7/BW125/CR4/5, frequency 915.0 MHz provisional, preamble 8, explicit header, CRC, sync 0x12, 10 dBm, 1 MHz SPI; SF9 control unused. XTAL/LDO/internal DIO2 RF-switch behavior requires physical confirmation.
- Antenna inventory says 915 MHz Moxon; actual feed/matching/orientation, distance/LOS, supply voltage/transients, permitted channel/EIRP/access/site authorization: NOT MEASURED / TBD.
- Physical packets attempted/received, PDR, ACK success, RSSI/SNR, retries, latency, resets, driver/CRC counts: NOT MEASURED. Physical NVS save/reboot/corruption fixture, radio initialization and RF tests: NOT RUN. No upload or serial-device opening occurred. Only the port list was read; it contained Bluetooth ports.
- Final status: M0/M1/M2 IN PROGRESS (software compile/host evidence available, physical gates pending); M3–M8 NOT STARTED. Software report contains a staged M3–M8 plan without implementation.
- Exact next task: inspect and document both boards/modules/pin continuity/supply/antenna/USB and permitted bench RF setup, then flash the same image, verify NVS/role/LLCC68/BUSY/DIO1/reset and capture >=200 HELLO attempts with both serial logs. M2 acceptance includes exact ACK echo, timeout/duplicate handling and measured success with explicit denominators. M3 may start only after those gates pass.
