# Error Log — Append Only

Track every build error, failed test, runtime fault, hardware issue, and
acceptance-blocking warning.

Do not delete closed issues. Change their status and append verification
evidence.

## Severity

- `CRITICAL`: unsafe electrical/RF condition, data corruption,
  uncontrolled transmission, or system cannot boot.
- `HIGH`: core radio link or protocol function fails.
- `MEDIUM`: degraded behavior, recovery failure, or repeatable test
  failure outside the main path.
- `LOW`: diagnostics, documentation, or non-blocking defect.

## Status

- `OPEN`
- `INVESTIGATING`
- `FIXED — UNVERIFIED`
- `CLOSED — VERIFIED`
- `WONTFIX` (must include rationale)

## Required issue template

``` markdown
## ERR-XXX — Short title

- First seen:

- Last updated:

- Iteration:

- Severity:

- Status:

- Component:

- Firmware / protocol version:

- Hardware and role:

### Expected behavior

### Observed behavior

### Reproduction steps

### Relevant logs
- Exact file path:

- Short excerpt:

### Diagnosis
- Confirmed root cause:

- Suspected causes not yet proven:

### Fix
- Files changed:

- Description:

### Verification
- Exact command/test:

- Result and exit code:

- Physical retest evidence:

- Remaining risk:

### Related issues / decisions
```

------------------------------------------------------------------------

## Initial issue state

No firmware build or physical radio test has been performed as part of
the documentation package. This is not evidence that the firmware is
error-free. Create the first issue when an actual error or
acceptance-blocking condition is observed.

## ERR-001 — Build tools require cache/process access outside sandbox

- First seen / updated: 2026-11-12 WIB; iteration 001; severity MEDIUM; status CLOSED — VERIFIED for dependency installation and host compiler probe.
- Component: development environment; firmware candidate 0.2.0-alpha.1 / protocol 1; physical hardware not involved.
- Reproduction: run installed PlatformIO `pkg install -e esp32c3_supermini` inside restricted sandbox: PermissionError for `<PLATFORMIO_HOME>\platforms.lock`. Launching installed 32-bit MinGW `g++ --version` in sandbox reports Illegal System DLL Relocation.
- Logs: `logs/build/iteration-001-dependencies.log`, `logs/build/iteration-001-dependencies-retry.log`; compiler launch result in tool transcript.
- Fix: use reviewed execution outside sandbox for normal package-cache writes and tool startup; no source workaround or SDK files manually modified.
- Verification: dependency install exit 0, RadioLib 7.8.1; host compiler probe exit 0, GCC 6.3.0. Build/test outcomes tracked separately. Physical retest N/A.

## ERR-002 — Native test source missing standard library includes

- First seen / updated: 2026-11-12 WIB; iteration 001; severity MEDIUM; status OPEN.
- Component: native test sources; firmware candidate 0.2.0-alpha.1 / protocol 1; physical hardware N/A.
- Reproduction: `<PIO_PYTHON> -m platformio test -e native` outside sandbox.
- Expected: compile and execute four test suites. Observed: brace-list iteration requires `<initializer_list>`; console test `std::snprintf` requires `<cstdio>`.
- Root cause: test files relied on transitive includes absent in GCC 6.3.0/Unity.
- Full log: `logs/test/iteration-001-native.log`.
- Fix / verification: pending explicit includes and rerun. No physical test performed.

### ERR-002 verification — iteration 002

Explicit `<initializer_list>` and `<cstdio>` fixed compilation. All four suites ran in `logs/test/iteration-002-native.log`. Status CLOSED — VERIFIED for compile errors. Command exit 1 is due to ERR-003. No physical evidence claimed.

## ERR-003 — Unity rejects zero-length array assertion

- First seen / updated: 2026-11-12 WIB; iteration 002; severity LOW; status OPEN.
- Component: `test/test_frame_codec/test_main.cpp`; firmware 0.2.0-alpha.1 / protocol 1; hardware N/A.
- Reproduction: native `test_payload_minimum_and_maximum` compares arrays with length 0.
- Observed: Unity reports "You Asked Me To Compare Nothing"; 43 other cases passed. PlatformIO also emits CTRL_BREAK_EVENT for the failed suite. Full log: `logs/test/iteration-002-native.log`.
- Root cause: misuse of Unity's nonempty array assertion, not a failed codec round-trip.
- Fix / verification: pending guard around empty content comparison; retain length and return-code checks.

### ERR-003 verification — iteration 003

Status CLOSED — VERIFIED. Guarding only empty array-content comparisons preserves boundary assertions. `PIO test -e native`, exit 0, all 44 cases passed in `logs/test/iteration-003-native.log`.

## ERR-004 — Firmware build saw incomplete header cleanup

- First seen / updated: 2026-11-12 WIB; iteration 001 firmware command / iteration 003 edit; severity MEDIUM; status FIXED — UNVERIFIED.
- Component: `src/app/firmware_runtime.h`; 0.2.0-alpha.1 / protocol 1; hardware N/A.
- Reproduction: firmware build overlapped removal of the transitive gateway header from ClientApp. Compiler reported GatewayApp undeclared in Runtime.
- Root cause: source was edited during compilation; Runtime required a direct gateway header include.
- Fix: Runtime explicitly includes gateway and client headers; shared generator alias moved into sequence module. Freeze source before the next build.
- Log: `logs/build/iteration-001-esp32.log`; exit 1. Verification: next ESP32 build pending.

## ERR-005 — RadioLib native USB CDC warning

- First seen / updated: 2026-11-12 WIB; iteration 001 firmware command; severity LOW; status OPEN (hardware verification).
- Component: RadioLib.h compile-time warning; 0.2.0-alpha.1 / protocol 1; both provisional C3 boards.
- Reproduction: build with ARDUINO_USB_CDC_ON_BOOT=1. RadioLib warns USB CDC output might stop after sleep.
- Log: `logs/build/iteration-001-esp32.log`.
- Diagnosis: M0–M2 firmware does not enter MCU sleep; warning is retained. Actual USB CDC versus USB-UART bridge must be checked for each Super Mini.
- Fix/verification: no suppression; document board-specific flags and verify sustained serial capture/no resets on both units. Compile warning alone does not block software candidate; physical serial acceptance remains unverified.

### ERR-004 verification — iteration 004

Status CLOSED — VERIFIED for the compile failure. Frozen-source `PIO run -e esp32c3_supermini` exit 0 in `logs/build/iteration-004-esp32.log`. No physical runtime behavior is claimed.

## ERR-006 — Metadata probe encoding/options and sandbox startup failures

- First seen / updated: 2026-11-12 WIB; iteration 004; severity LOW; status CLOSED — VERIFIED.
- Component: host tool probes; firmware 0.2.0-alpha.1 / protocol 1; hardware N/A.
- Reproduction: redirected `PIO pkg list -e esp32c3_supermini` under cp1252 raises UnicodeEncodeError (exit 1); attempted `--json-output` is unsupported (exit 2). RISC-V compiler startup inside sandbox failed before execution with StandardOutputEncoding error.
- Logs: `logs/build/iteration-004-packages.log`, `logs/build/iteration-004-packages-utf8.json` (error text), `logs/build/iteration-004-compiler.log` (empty startup-failure capture).
- Fix: use ordinary package list with process-local PYTHONUTF8=1 and reviewed execution outside sandbox; collect actual compiler versions outside sandbox.
- Verification: package list exit 0 in `logs/build/iteration-004-packages-final.log`; compiler exit 0 in `logs/build/iteration-004-compiler-retry.log`. No firmware workaround or dependency version change.

## ERR-007 — Driver header-error filter needs valid-header qualification

- First seen: 2026-11-12 13:43 WIB; iteration 004 review; severity MEDIUM; status OPEN.
- Component: `src/common/radio/radio_driver.cpp`; firmware 0.2.0-alpha.1 / protocol 1; hardware NOT TESTED.
- Static reproduction: evaluate a receive IRQ containing RX_DONE + HEADER_ERR + HEADER_VALID and no CRC_ERR. The wrapper rejects every HEADER_ERR, while pinned RadioLib readData rejects HEADER_ERR only without HEADER_VALID to avoid stale header-error false alarms.
- Root cause: driver pre-filter was stricter than the library's documented implementation. This is a static defect; no observed RF failure count is claimed.
- Fix: qualify header failure with absence of HEADER_VALID, retain unconditional CRC_ERR rejection and require a meaningful completion/error before processing. Rebuild and record review; physical IRQ/RF checks remain pending.

### ERR-007 verification — iteration 005

Status CLOSED — VERIFIED for the source defect. The filter now matches pinned RadioLib's condition; final static review is in `logs/build/iteration-005-static-review.log`, firmware build exit 0 in `logs/build/iteration-005-esp32.log`, native 44/44 pass exit 0 in `logs/test/iteration-005-native.log`. Native suites do not exercise the radio. Physical header/CRC IRQ behavior and RF performance remain NOT TESTED under the M1/M2 hardware gates.
