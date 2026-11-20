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

## ERR-008 — Synchronous operations limit cooperative-loop deadlines

- First seen: 2026-11-12, iteration 006 read-only audit; severity MEDIUM; status OPEN (timing limitation, no physical failure reproduced).
- Source evidence: NvsConfigStore::save calls Preferences::putBytes; installed Preferences commits flash synchronously. RadioLib Module::SPItransferStream yields/waits on BUSY with its own timeout. Application TX/ACK software deadlines are only evaluated after these calls return.
- Reproduction: inspect actual SDK/library sources and the direct calls recorded in SOFTWARE_AUDIT.md. No assertion that a particular flash/BUSY duration occurred on a device.
- Disposition: preserve M2 behavior; measure/fault-test these boundaries before claiming hard real-time/nonblocking guarantees. App fake transport tests deliberately exclude them. Physical verification NOT RUN.

## ERR-009 — RadioLib default SPI path allocates scratch buffers

- First seen: 2026-11-12, iteration 006; severity LOW; status OPEN (dependency allocation risk; no allocation failure observed).
- Source evidence: RadioLib 7.8.1 BuildOpt.h defaults RADIOLIB_STATIC_ONLY=0; Module::SPItransferStream uses new[]/delete[] scratch buffers. Fixed application buffers are not a whole-firmware allocation-free guarantee.
- Disposition: retain the pinned driver/memory mode for this behavior-preserving seam task. A future static-buffer change must review all command bounds and stack budget; no dependency upgrade or allocation-failure behavior claimed verified.

## ERR-010 — Logger capacity check does not prove nonblocking SDK I/O

- First seen: 2026-11-12, iteration 006; severity LOW; status OPEN (SDK timing limitation).
- Source evidence: installed HWCDC availableForWrite/write use a mutex timeout (default 100 ms), and write can retry timed ring-buffer sends. A capacity check can race with SDK state/backpressure.
- Disposition: correct documentation's hard nonblocking claim; test the production queue with deterministic capacity/short writes. USB/UART mutex timing and backpressure latency remain unmeasured. No serial setting changed in this task.

## ERR-011 — Production log-format macros hidden by legacy MinGW header

- First seen: 2026-11-12 14:37 WIB, iteration 006; severity MEDIUM; status OPEN.
- Reproduction: compile actual ClientApp and SerialLogger via native target. MinGW 6.3.0 `<inttypes.h>` hides PRIu32/PRIu64/PRId64 in C++ unless __STDC_FORMAT_MACROS is defined; compilation fails before any tests run.
- Log: `logs/test/iteration-006-native.log`; native command exit 1, six errored suites/zero executed cases.
- Fix proposed: use standard C++ `<cinttypes>` in the two production .cpp files, preserving log format and state behavior. Verification pending.

### ERR-011 verification — iteration 007

Status CLOSED — VERIFIED. ClientApp/SerialLogger now include C++ `<cinttypes>`; no format string or application statement changed. `pio test -e native` through installed Python, exit 0, 92/92 pass in `logs/test/iteration-007-native.log`; actual formatter prefix/N/A checks pass. ESP32 build exit 0 in `logs/build/iteration-007-esp32.log`. No physical serial test performed. ERR-008/009/010 remain explicit limitations, not claimed fixed by the fake transport.

## ERR-012 — Added historical notice used mixed line endings

- First seen: 2026-11-12, iteration 007 diff review; severity LOW; status FIXED — UNVERIFIED at this checkpoint.
- Reproduction: Git no-index --check with core.autocrlf=false reported two CR-at-EOL trailing-whitespace lines in the newly added IMPLEMENTATION_REPORT historical notice; exit 3 (differences plus whitespace diagnostics).
- Exact failed-check output: `logs/build/iteration-007-diff-check-initial.log`. Source/firmware/tests were unaffected.
- Fix: normalize the two added CRLF notice lines to the existing report's LF style; historical content retained. Final diff check/review evidence will be in `logs/build/iteration-007-review.log` and `iteration-007-diff-check.log`.

### ERR-012 verification — iteration 007

Status CLOSED — VERIFIED. The original historical report body is retained byte-for-byte with only the two LF notice lines inserted. Final Git no-index --check returned 1 (differences expected) with no whitespace output; explicit added-line whitespace/conflict checks passed. See `logs/build/iteration-007-review.log`. No firmware source changed.

## ERR-013 — Migration equivalence checker renamed a preserved storage literal

- First seen: 2026-11-12, iteration 008; severity LOW; status CLOSED — VERIFIED.
- Reproduction: the initial static comparison mechanically renamed the legacy namespace inside its expected string literal, even though the migration intentionally retains those bytes. It reported an unexpected NVS source change; no firmware/runtime defect was reproduced.
- Initial output: `logs/build/iteration-008-behavior-equivalence-initial.log`, checker exit 1.
- Fix: protect the original literal in the expected model, compare numeric compatibility bytes directly, then compare normalized source bodies after only the audited namespace/macro substitutions. No source repair was needed.
- Verification: checker exit 0 in `logs/build/iteration-008-behavior-equivalence.log`, 35 production/header/test files covered; wire/profile/versions/state logic/storage calls remain equivalent. Physical NVS behavior remains unverified.

## ERR-014 — Staged scanner confused HTTPS URLs with drive paths

- First seen: 2026-11-12, iteration 008; severity LOW; status CLOSED — VERIFIED after corrected scan.
- Reproduction: the initial staged-blob checker returned 1 and classified 14 files containing public HTTPS links as workstation paths because an unbounded drive-letter expression matched a URL suffix. The evidence contained no actual private path.
- Fix: require a drive-path token boundary; retain the identity, credential, private-address, filename and source-fingerprint checks. No firmware or public URL changed.
- Initial/corrected output: `logs/build/iteration-008-staged-scan.log`; corrected results: `logs/build/iteration-008-staged-review.json`.
- Two attempted reads of nonexistent Chinese build/error record counterparts also failed; the directory inventory confirms English holds those canonical chronological records. Chinese progress/decision documentation carries the migration summary.

### ERR-006 recurrence — iteration 008 final log inspection

A final Python `Path.read_text()` inspection without an encoding returned exit 1 while reading the UTF-8 ESP32 log under the Windows cp1252 default: UnicodeDecodeError, byte 0x90 at offset 15464. This was a log-reader failure, not a firmware build/test failure. Re-reading with `Get-Content -Encoding utf8 logs/build/iteration-008-esp32.log -Tail 11` exited 0 and showed SUCCESS / one environment succeeded. Keep explicit UTF-8 for evidence readers; the original successful log remains unchanged.

### Iteration 009 inspection note

An artifact-presence probe during the still-running ESP32 compile found no firmware.bin yet. It was a premature inspection, not a compiler error; the subsequent build exited 0 and generated the application/factory binaries (`logs/build/iteration-009-esp32.log`). No repair was needed. Known ERR-005 CDC warning remains present; physical serial verification is still open.

## ERR-015 — Captured PlatformIO display whitespace failed staging check

- First seen 2026-11-12, iteration 009; severity LOW; status FIXED — UNVERIFIED at this checkpoint.
- Reproduction: `git diff --cached --check` returned 2 for trailing display spaces in ESP32 package/metrics output and a final blank line in the toolchain log. Native/ESP32 commands had already passed; source was unaffected.
- Initial diagnostics: `logs/build/iteration-009-diff-check-initial.log` (only incidental display whitespace normalized); exact original command outputs remain privately retained outside publication.
- Fix: normalize line endings/trailing display spaces/blank EOF in the three new command logs; retain all output lines, codes, values, warnings and PASS/FAIL evidence. No historical log or production/test source modified.
- Verification: final staged check and publication audit pending below; no repeat compile needed for log formatting.

### ERR-015 verification — iteration 009

Status CLOSED — VERIFIED. Corrected `git diff --cached --check` exited 0 without diagnostics. Staged-blob security/history/source-hash/link/fact checks pass (`logs/build/iteration-009-review.json`); all 43 compiled source/test/config/version entries match. Only publication display formatting changed; no source rebuild/retest was necessary.

### Iteration 010 verification checkpoint

No firmware/native test failure or new compiler warning occurred in the first verification run: ESP32 exit 0, native 219/219 exit 0 (`logs/build/iteration-010-esp32.log`, `logs/test/iteration-010-native.log`). Warning-message comparison against iteration 009 retains only known ERR-005 USB CDC warning. ERR-008 synchronous NVS/RadioLib operations, ERR-009 dependency allocation and ERR-010 SDK serial timing remain open; store fake does not validate those dependencies. The preliminary optional lookup of a not-yet-existing fake file returned 1; the inspected inventory confirmed absence before creating it, with no source/build fix required. No physical NVS/RF test or flash-failure observation occurred.

## ERR-016 - Restricted shell read requests stalled before output

- First seen 2026-11-12, iteration011; severity LOW; environment issue, root cause UNKNOWN; status WORKAROUND VERIFIED, underlying cause unresolved.
- Reproduction: restricted request-attachment/repository reads with default and login=false invocation returned running sessions but no output; interrupted both with Ctrl-C, each exit1. Whether individual shell commands began is NOT AVAILABLE; no source was modified by these read requests.
- Normal host-shell retry of the same read-only inspection exited0 and exposed the expected baseline. Subsequent work uses reviewed host execution for existing SDK/tool access. No approval review rejection or bypass was involved.
- Evidence: `logs/build/iteration-011-runner-preflight.log`; no build/test/physical result inferred.

## ERR-017 - Native Unity did not enable requested 64-bit assertions

- First seen iteration011, 2026-11-12; severity LOW, test environment; status FIXED - UNVERIFIED at this checkpoint.
- Reproduction: native runtime suite compiles with TEST_ASSERT_EQUAL_UINT64 while Unity on this 32-bit MinGW defaults to no UNITY_SUPPORT_64. Macro expands to UnityStrErr64 failure without reading expected/actual, producing -Werror unused-variable for rx. Runtime tests did not execute; all219 old cases passed.
- Evidence: `logs/test/iteration-011-native.log` exit1; actual installed Unity unity_internals.h support guard and unsupported-64 macro inspected.
- Fix: define UNITY_SUPPORT_64 for the native environment (framework and tests consistently), retain full-width comparisons and every case; add fixture lifecycle invariants. Production and ESP32 flags unchanged. Verification pending corrected native rerun; no application defect claimed.

### ERR-017 verification - iteration011

Status CLOSED - VERIFIED. With native UNITY_SUPPORT_64 applied consistently, the unchanged full-width assertions compile and all47 runtime cases plus219 regressions pass, exit0 (`logs/test/iteration-011-native-retry.log`). No case or assertion removed; initial failed log retained. No firmware policy fix or physical claim.

## ERR-018 — Probe launcher setup failures

Iteration012, environment/tooling, CLOSED — VERIFIED for version probes. Initial Python subprocess pio --version could not launch (WinError2); exit1 before probes/audit saved. Get-Command pio confirmed absent from PATH; installed <PLATFORMIO_HOME>/penv/Scripts/pio.exe exists. A subsequent evidence script failed parsing a non-ASCII bytes literal (SyntaxError, exit1; no commands executed). Corrected UTF-8 encoding/resolved executable yields four exit0 version probes in logs/build/iteration-012-toolchain.log. No dependency installation/change or production defect. Baseline ELF probe output/exit separately retained in iteration-012-memory-baseline.log.

### ERR-018 ELF probe correction

Initial GDB printf expressions lost inner quotes in Windows argument handling (exit1, iteration-012-memory-baseline.log). Replaced quoted printf with plain p sizeof expressions; actual corrected result in iteration-012-memory-baseline-retry.log. No source change from this probe.

## ERR-019 — Unbounded session-ID generation

REPRODUCED, correction pending. Baseline actual freshBootId on zero, previous100, alternating0/100 did not return before a native-only guard interrupted64 raw calls. python tools/reproduce_boot_id_liveness.py exit0 proves rejection continuation, without an infinite run; logs/test/iteration-012-boot-probe.log. Two actual runtime boot regression tests also fail guard64. Root cause: rejection loop lacks attempt bound and return-failure contract. Affected boot and sequence rotation; no physical entropy failure claimed.

## ERR-020 — Stale runtime timestamps across synchronous dependencies

REPRODUCED, correction pending. pio test -e native -f test_runtime_reliability exit1, logs/test/iteration-012-reproduction.log: delayed RX read accepts ACK beyond deadline; delayed finishTX anchors ACK window too early; save/stop can consume100ms reboot interval; reset check ignores elapsed serial waits. Test stimuli READ/FINISH_TX20us, WRITE/STANDBY200000us, output wait150000us are simulated inputs, not hardware measurements. Root cause: one pre-poll micros snapshot reused throughout loop. Initial wrap test additionally used a Client fixture whose dueHELLO started before reboot (existing app-before-command policy); change fixture to Gateway to isolate reset wrap, rerun before production correction. No production change to that ordering proposed.

### ERR-019/020 verification — Iteration012

CLOSED — VERIFIED for reproduced firmware paths. Corrected focused10/10 exit0, then all290/290 native cases and ESP32 build exit0. No old266 test cpp/name/result changed. Raw RNG bound8, zero sentinel/counter preservation/app1s retry/TX suppression verified at boot/rollover/recovery. Post-poll/app, post-stop arming and post-drain reset timestamps fix reproduced staleness, including wrap. logs/test/iteration-012-focused.log, iteration-012-native.log and logs/build/iteration-012-esp32.log; delays are simulated. No hard real-time/entropy/hardware claim. ERR-018 corrected GDB retry exit0; an incidental rg filename wildcard lookup was rejected on Windows, corrected by explicit filenames with no source impact.

## ERR-021 — Unbounded upstream TX BUSY-ramp dependency path

OPEN — SOURCE-IDENTIFIED RISK, NOT PHYSICALLY REPRODUCED. RadioLib7.8.1 PhysicalLayer.startTransmit -> SX126x.launchMode TX has a separate while(BUSY) yield loop at SX126x.cpp1121–1123 without timeout, after setTx returns. Module's pre/post transfer1000ms checks do not bound this later loop; driver2s deadline cannot preempt it. SPI transaction locks may also wait portMAX_DELAY. Reproduce on hardware only after safe assembly/fault-injection setup; native fakes do not execute these dependency internals. No third-party patch/flag/version change in012; source fingerprints and inventory in DEPENDENCY_BUDGET_AUDIT.md. Hard runtime latency acceptance remains blocked on disposition/measurement; audit/software regression completion does not close this risk.

### ERR-008/009/010 dependency clarification — Iteration012

Still OPEN. Installed IDF5.5.5 nvs_commit is no-op after validation; set_blob does flash work under NVS locks. HWCDC available/write mutex default100ms, connected write consecutive-failure cap20; no measured end-to-end guarantee. RadioLib repeated two-buffer new[] scratch per command, no local recoverable allocation failure handling; STATIC_ONLY moves536 bytes to automatic buffers and leaves other SDK/HAL allocations, so unchanged. Static size is not peak heap/stack. See audit for exact sources, call sites and measurement plan. ERR-005 USB physical warning remains.

## ERR-022 - Capture interruption reported as successful completion

REPRODUCED, correction pending, Iteration013. Exact012 tools/capture_bench.py executed with fake serial objects and an injected KeyboardInterrupt: exit0, errors=[], no interruption marker, no chunk timestamp files. No physical port opened. Evidence logs/test/iteration-013-capture-reproduction.log. Root cause: interrupt except branch passes without recording incomplete state. Missing timestamp/offset and final-fragment evidence also prevents reliable CSV extraction; bounded4096-byte raw streaming itself is preserved.

### Iteration013 source-access/tool observations

Legacy vendor PDF URLs returned HTML/failed fetch; current official product-page download succeeds (hash/pages in readiness record). No hardware verification inferred. Installed esptool5.3 wrapper help emits a deprecation notice for esptool.py; supported syntax used for offline help only, no flash command executed. Read-only port enumeration exit0/no ports found. Physical dependencies remain unverified.

### ERR-022 verification - Iteration013

CLOSED - VERIFIED for host capture behavior.13/13 focused fake-port tests exit0 (logs/test/iteration-013-capture.log). Ctrl-C now records INTERRUPTED/exit130; IO/short-write/unjoined/finalization errors cannot report COMPLETE, hard termination retains initialRUNNING. Byte/hash/offset/time/tail evidence and single-board mode added for actual acceptance requirements. No real port opened, no firmware/native change; all290 native and ESP32 reruns exit0. Raw capture can still lose bytes before host read; final status/backpressure/reset/fragment review remains mandatory, not an automatic RF PASS.

## ERR-023 - SDK startup can format NVS before Runtime

OPEN - SOURCE-IDENTIFIED PRESERVATION RISK, NOT PHYSICALLY EXERCISED. Installed Arduino3.3.11 initArduino (esp32-hal-misc.c331-337) calls nvs_flash_init before FirmwareRuntime and erases the NVS partition/retries on NO_FREE_PAGES or NEW_VERSION_FOUND. Store-level native tests do not execute this SDK startup. A non-overlapping upload alone cannot promise retention across first application boot. Disposition in013 is mandatory private full-flash/NVS backup and verified recovery before that boot; secure/encrypted/incompatible layouts halt the plaintext procedure. No SDK/firmware patch and no erasure performed. Physical corruption/SDK-formatting/power-loss/restore evidence remains NOT RUN; see BENCH_RUNBOOK single-board branch.

### ERR-021 / hardware acceptance hold - Iteration013

Still OPEN; exact path/conditions/watchdog/defaults/options documented in DEPENDENCY_BUDGET_AUDIT.md. No source risk is reclassified as a physical failure. Unattended/stuck-BUSY recovery acceptance held; normal attended observations cannot close it. All board/revision/continuity/USB/supply/oscillator/antenna/regulatory and controlled RF/NVS fixture gates remain unverified. Incremental013 build emits no warning, but ERR-005 and other dependency/physical limits are not closed.

## ERR-024 - Publication auto-review did not recognize attached authorization

Iteration013, environment/approval, status RETRY PENDING after evidence check. Combined preflight/commit/push request was rejected before CreateProcess; no command/commit/push executed. Review reason: no current user publication authorization recognized, citing the conditional no-commit rule. Read-only recheck of the current user attachment Stage8 explicitly confirms "After successful validation, commit and automatically push" to the authorized Adaptive-LoRa remote and the recommended docs commit. HEAD remains9341e7c0016c66fd9316d489ccfca00a646ae2cb on013, reviewed work preserved. A narrower explicit publication request will include that evidence; no force/merge/PR or indirect bypass is authorized.

### ERR-024 verification - Iteration013

CLOSED - VERIFIED. After read-only recheck of the current user attachment Stage8, explicit commit and push requests including that authorization were approved. Requested implementation commit/push each exited0; independent ls-remote exit0 matches local HEAD. No rejected command executed, no indirect bypass/force/merge/PR. Actual verified refs/commands in iteration-013-publication-receipt.json; source/tests unchanged.

## Iteration013 main-integration environment and verification checkpoint

2026-11-13. No ESP32 build, native test or capture test failure: required commands all exit0,290/290 native and13/13 fake-port capture PASS. Logs/results use the `iteration-013-main-integration-` prefix under logs/build and logs/test. No merge conflicts: main-only merge commits introduce no file changes relative to the common ancestor.

ERR-001/016 environment recurrence: the initial restricted batched inspection exited1. PowerShell reported InitializeDefaultDrives/FileSystem provider failure; git fetch reported cannot open .git/FETCH_HEAD: Permission denied, and ls-remote reported getaddrinfo thread failed to start. Approved host-shell retry of fetch/ls-remote exited0 and verified both expected heads; no repository content was modified by the failed read/fetch attempt. Workaround VERIFIED; restricted-runner cause remains unresolved.

ERR-006 encoding recurrence: initial Python document dump exited1 with UnicodeEncodeError under Windows cp1252 on U+2192 after successful read-only Git/tree checks. Retried with `python -X utf8 -`, exit0; subsequent evidence reads/writes use explicit UTF-8. No document bytes or firmware behavior changed by the failed inspection.

Existing ERR-005/008/009/010/021/023 and physical NVS/reset/USB/SPI/IRQ/RF/entropy/heap/stack holds remain open. Software integration is not a physical reproduction, dependency timing guarantee or hardware acceptance. Automatic branch deletion is false; historical branches must remain after merge.

## ERR-025 - Iteration014 audit tooling and native fixture setup

2026-11-17 client date. Initial Windows rg paths with literal *.h/*.cpp components returned os error123/exit1; directory search with -g patterns succeeded. Unauthenticated vendor GitHub tree retrieval returned HTTP403/rate limit; authenticated gh api succeeded. Bundled/system Python had no fitz; first unpinned renderer install was cancelled (exit1) after selecting unnecessary optional dependencies, and isolated PyMuPDF1.26.6 --no-deps installed successfully. An attempted private run_validation.py invocation before that helper existed exited2; no validation ran from it. The helper was then created under ignored .pio and used for actual recorded commands. No firmware or dependency library was changed by these probes.

First migrated S3 build exit0, but native compilation exit1 because test/support/platform/RadioLib.h defined RADIOLIB_IRQ_RX_DEFAULT_FLAGS twice. Evidence logs/test/iteration-014-migrated-native.log and -result.json (recorded at command completion). The duplicate literal was removed, retaining the expression verified against pinned PhysicalLayer.h. Firmware was unaffected; all tests/assertions remain. Status: corrected fixture, full verification pending.

### ERR-025 verification - Iteration014

Corrected native fixture is CLOSED - VERIFIED. `pio test -e native` exit0,302/302 PASS (all290 original names/order retained plus12 adapter cases); capture13/13 exit0; S3 rebuild exit0. Full logs/results: iteration-014-retry-native/capture under logs/test and retry-esp32 under logs/build. Initial migrated-native failure is retained. No test assertion removed; one expected selected-driver label changed for the new target. Final S3 logs contain no compiler warning/error matches.

### ERR-021 / ERR-023 migration assessment - Iteration014

Both remain OPEN. SX1262 uses the same pinned SX126x.launchMode BUSY-ramp loop after setTx; driver deadlines cannot preempt it. V3.2 changes neither this dependency nor its watchdog policy. SDK pre-Runtime NVS initialization/formatting is still present. Require same-board backups before app boot and controlled recovery evidence. Previous HWCDC-specific ERR-005 context is historical after the UART0 selection, without claiming the old physical issue was fixed. ERR-008/009 synchronous operations/allocations remain; ERR-010 now requires UART mutex/uart_write_bytes assessment and physical serial timing rather than transferring old CDC observations. No physical failure/measurement was produced in this iteration.

### ERR-025 exit-code clarification

The earlier helper-note exit2 was not the observed tool wrapper status. The missing-file batch returned exit1 and Python printed Errno2 (file not found); that OS error number must not be presented as the measured process exit. No test ran from that invocation. The actual recorded build/native/capture exit codes in their result JSON remain unchanged.


## ERR-026 — Iteration 015 new Chinese research link

REPRODUCED; correction awaiting rerun. The first documentation checker suite
exited 1: five cases passed and two repository-fixture cases failed because
new docs/zh-CN/TDMA_FEASIBILITY.md linked to a nonexistent local
MILESTONE_TASKS.md. Running the checker directly also exited 1 with that
single broken link. Reproduce with `<PIO_PYTHON> -m unittest discover -s tools
-p test_check_consistency.py -v` or `<PIO_PYTHON> tools/check_consistency.py`.
Correction points to the existing canonical ../en/MILESTONE_TASKS.md, without
creating a redundant translated roadmap. Initial complete output is preserved
in logs/test/iteration-015-verification-consistency-tests.log and its result
JSON. No firmware/test assertion was weakened; production code is unchanged.

### Iteration 015 tool-access and inspection notes

One authorized validation launcher was rejected before execution because
automatic approval review hit an account usage limit, explicitly not an unsafe
action finding. The resumed authorization check succeeded; the launcher was
then created and executed. That rejected attempt produced no build/test result.
An inspection glob passed as a Windows rg filename returned OS error 123;
explicit paths corrected the lookup. A probe for a Chinese BUILD_MEMORY file
reported it absent; canonical chronological memory remains English. Neither
inspection issue changed firmware or invalidated executed test results.


### ERR-026 second-run checker correction

The link correction removed the broken-target error. The second suite still
exited 1 (five passed/two failed): the newly generalized firmware-reference
regex also matched the suffix in Esp32Firmware and consumed following prose
as a version. Added a word boundary to the English field name; validation
of explicit firmware/protocol fields remains required. Both failed run logs
are retained, the second under iteration-015-corrected-consistency-tests.
This is a checker defect, not a firmware defect; final rerun follows.


### ERR-026 verification — CLOSED, 2026-11-18

Final consistency suite exit 0, 7/7 PASS; direct checker exit 0. Logs/results:
logs/test/iteration-015-final-consistency-tests* and final-consistency*.
All negative fixtures remain asserted; both failed attempts are preserved.

### ERR-006 recurrence — package inventory encoding, Iteration 015

Initial `pio pkg list -e heltec_wifi_lora_32_V3` exited 1 with
UnicodeEncodeError while printing its tree in a legacy console. Complete
output remains in logs/build/iteration-015-toolchain.log. Repeating S3 and
native inventories with PYTHONUTF8=1/PYTHONIOENCODING=utf-8 each exited 0;
logs/build/iteration-015-packages-utf8.log. This was metadata display, not a
firmware build failure or dependency upgrade. Package listing includes shared
installed packages; it is not a claim that all listed packages are linked.

### Iteration 015 unresolved dependency and hardware findings

ERR-008/009/010/021/023 remain OPEN. Current S3 watchdog, UART, allocation and
NVS source review/resources are appended to DEPENDENCY_BUDGET_AUDIT.md;
BENCH_RUNBOOK.md specifies the unexecuted ERR-021 fixture and recovery gate.
Native eviction/reboot characterization is expected M2 behavior; it limits
future DATA delivery guarantees, without changing production policy. No new
production defect was reproduced and no physical failure was observed.


### ERR-015 recurrence — Iteration 015 staged log formatting

Second commit staged check `git diff --cached --check` exited 2 because the
new packages-utf8 log had an extra blank line at EOF. Original bytes retained
privately in .pio/iteration-015/packages-utf8-original.log; normalized only
trailing display whitespace/EOF, preserving package output and exit codes.
Restaged check follows; this is publication formatting, not a build/test failure.


### ERR-015 Iteration 015 verification — CLOSED

Restaged `git diff --cached --check` exited 0. Second scoped commit ba56667
succeeded. Only the new log EOF changed; all actual package records remain.


## ERR-027 — Iteration 016 native helper name collision

Initial `pio test -e native` exited 1. The OLED fixture helper `resetTest`
collided with a Unity symbol. Renamed the test-only helper to
`resetOledFixture`. Corrected native run exited 0 with 316/316 passing.
See logs/test/iteration-016-first-native.log and corrected-native.log with
their iteration prefixes. No production failure was observed.

## ERR-028 — Iteration 016 shared PlatformIO build output

The first OLED build exited 1 when generated FrameworkArduino dependency
paths disappeared during overlapping PlatformIO operations. The missing
files occurred during framework compilation. A sequential rebuild of both
variants is underway. Initial output remains in
logs/build/iteration-016-first-oled.log. The first display-disabled build
also pulled in Wire through dependency discovery despite the preprocessor
guard. The default environment now excludes Wire explicitly. The separate
OLED environment includes it.

### Iteration 016 physical evidence and dependency holds

The host listed Bluetooth ports only. No device ROM, OLED, NVS, power,
link or RF measurements were taken. ERR-008/009/010/021/023 stay open.
Arduino Wire accepts an I²C timeout, while its SDK/locks still need measured
latency. The new display work therefore runs last and is suppressed in
known TX, ACK-wait and reboot windows. RX IRQ arrival during a transfer
remains possible. Do not claim isolated bench timings from native tests.


### ERR-028 verification — CLOSED for the build

Running the display-disabled and OLED-enabled builds sequentially completed
with exit 0. The disabled image has no Wire or display service symbols.
The opt-in image links both. Keep the retained first build and corrected
logs for comparison. Physical Wire and OLED timing remain unmeasured.

A restricted Windows CIM retry reported access denied. An approved read-only
CIM enumeration succeeded and again found Bluetooth serial ports only. The
runtime has not been connected to a board.


### Iteration 016.1 bench status and ERR-008 clarification

`pio device list` exited 0 and listed six Bluetooth virtual ports. It did
not show a CP2102 port. The transcript does not diagnose a cable or driver
fault. Hardware instance IDs are sanitized in logs/test/iteration-016.1-verification-ports.log.
No port was opened. Windows troubleshooting is in BENCH_RUNBOOK.md.

Source review found no reproduced OLED/radio defect. Wire's 3 ms timeout
covers individual backend operations and leaves lock waits outside the
budget. Gateway DIO1 service can be delayed during an RX-overlapping display
write. Measure it as described in DEPENDENCY_BUDGET_AUDIT.md. ERR-008,
ERR-021 and ERR-023 are still open. One repository inspection used the
unsupported Windows rg filename glob README* (OS error 123). The explicit
README paths corrected that lookup. All required builds and tests exited 0.

## ERR-029 — Iteration 019 native timing and serial fixtures

First `pio test -e native` exited 1. The old schema-1 config fixture treated
5,000 ms as the default. OLED and runtime fixtures still advanced to 1 s
ACK deadlines. The new scheduled fixture expected `result` immediately
after the event field, although the logger inserts identity and PHY fields.
PlatformIO also printed `Program received signal SIGINT (Interrupt)` for
the failing config-store suite. Full output is preserved in
`logs/test/iteration-019-verification-native.log`.

The next run kept the old NVS fixture explicitly, advanced timing to the
restored boundaries and matched diagnostics by record. It exited 1 with two
scheduled log assertions still failing because two simulated boards wrote
partial records to the same UART shim. PlatformIO reported SIGINT for that
suite. See `logs/test/iteration-019-final-native.log`. Serialize the two
fake UART drains and keep each complete record. The release run exited 0
with 333/333 cases, no failures or skips.

## ERR-030 — RadioLib retains low-power OCP after 22 dBm PA setup

Inspect pinned RadioLib 7.8.1 `SX126x.cpp` LoRa initialization and
`SX126x_config.cpp::setOutputPower`. Common initialization sets 60 mA, and
the PA change restores the previous OCP register. Semtech's SX1262
high-power `SetPaConfig` default is 140 mA. The firmware now explicitly
sets 140 mA after successful `begin()`. An injected OCP error suppresses
header, CRC and IRQ initialization. The three release builds and adapter
tests passed. Actual power, current and supply headroom still need a loaded
board measurement. ERR-021 and ERR-023 remain open.

### Iteration 019 audit tooling notes

PowerShell rejected a brace-expanded filename list; use separate explicit
paths. Windows `rg` rejected `README*` and a test directory argument ending
in `*`; use explicit paths or a `-g` filter. The first `Get-ChildItem`
probe for `.github/workflows` found no directory. A probe for an Iteration
018 scenario filename also found no file at that name. These lookups had no
effect on source or validation. The first staged whitespace check found a
trailing blank line in the Chinese architecture doc. Removing it cleared
the check. No physical errors were injected on a board.
