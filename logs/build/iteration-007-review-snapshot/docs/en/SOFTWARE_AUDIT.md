# M0–M2 software audit and verification

## Read-only audit baseline — before source edits

Baseline: firmware 0.2.0-alpha.1 / protocol 1, iteration-005 evidence (ESP32 compile and 44 native tests). Canonical English records and GatewayApp, ClientApp, FirmwareRuntime, RadioDriver, FrameCodec, DeviceConfig, SequenceTracker, console, logger and NVS persistence were inspected before editing source. This workspace still has no Git repository. An exact pre-edit snapshot is saved in `logs/build/iteration-006-baseline/` with SHA-256 inventory `logs/build/iteration-006-baseline-manifest.json`; Git no-index comparison will be used without initializing Git or making commits.

Inventory correction from the user: two RA-01SCH/LLCC68 modules, one ESP32-C3 Super Mini, second MCU not purchased, Moxon not fabricated, no assembled radio pair. Earlier two-unit/antenna availability statements described the intended setup and are superseded for current inventory. All physical NVS, GPIO, IRQ and RF behavior remains unverified. This task is software-only.

## Findings and disposition

| Area | Initial finding | Disposition |
|---|---|---|
| Architecture/testability | Apps and logger include RadioDriver's Arduino/RadioLib-dependent header; current native target excludes both production apps. Helper tests alone cannot verify application transitions. | Add a six-method transport seam; execute the actual app .cpp files and real codec/tracker/logger with host adapters. |
| GatewayApp | Decode/network/destination/type validation precedes acceptance; records identity before ACK so ACK loss can be handled by idempotent re-ACK. Receive-start recovery has a three-failure/one-second bound. | Exercise these paths through fake transport; preserve policy unless a failing regression reproduces a defect. |
| ClientApp | Stop-and-wait HELLO, timeout from TX completion, strict echoed identity, no automatic retries. Uses 64-bit timer for reporting interval and wrap-safe 32-bit ACK/recovery deadlines. | Exercise boundaries, lost/late/stale ACK, reboot and full sequence exhaustion using virtual time. |
| FirmwareRuntime | Fresh boot validates before radio initialization/app construction; only one optional role is emplaced. Staged save does not change the running role; reboot suppresses new requests and waits for TX completion/error. | Static audit only in this change; runtime/NVS/boot control is not claimed executed by app integration tests. |
| FrameCodec | Checks pointer/min/max/exact lengths before payload copy; explicit LE fields, bounded local frame, only publishes decoded output on success. ACK short-circuit length check precedes payload reads. | No out-of-bounds defect found in inspection; retain existing tests and add malformed frames through production apps. This is not a formal memory-safety proof. |
| DeviceConfig/NVS | Complete validator plus one 24-byte explicit blob with integrity check and readback; writes only on operator save. Failed decode preserves caller output; Runtime's fresh output is UNSET. | Add valid-integrity incompatible-blob tests to distinguish semantic rejection from hash rejection. Preferences/flash behavior remains untested. |
| Blocking operations | Preferences::putBytes calls nvs_set_blob/nvs_commit synchronously. Pinned RadioLib Module::SPItransferStream waits/yields for BUSY with timeout. Software TX deadline is only checked when control returns to poll. | Record timing limitation; fake transport deliberately does not verify BUSY/flash latency. No scheduler/flash-worker refactor without measured need. |
| Serial blocking | Installed Arduino HWCDC::availableForWrite/write take a mutex using a default 100 ms timeout; write can perform repeated timed ring-buffer sends. The logger's capacity check does not establish a hard nonblocking guarantee. | Correct documentation's guarantee. Test queue/backpressure/short-write behavior using real logger logic; actual USB timing remains open. |
| Dependency memory | RadioLib BuildOpt defaults RADIOLIB_STATIC_ONLY=0; Module::SPItransferStream allocates SPI scratch arrays with new[]. Project fixed buffers do not imply an allocation-free dependency hot path. | Record residual allocation risk. Do not silently change RadioLib memory mode/stack use in a behavior-preserving seam task. |
| ISR/race | ISR only sets a volatile flag. Poll samples then clears it; a latched high DIO1 is also sampled on the next pass, mitigating a lost software edge under that hardware assumption. | No atomicity/hardware-timing proof claimed. BUSY/DIO1/SPI and ISR races require later driver/hardware checks. |
| Replay/session bounds | Eight sessions with a 64-sequence window; older retained sequences are stale, evicted sessions and gateway reboot lose history. Boot-ID generator assumes a functioning ESP32 random source; its rejection loop is unbounded if that source repeats only forbidden values. | Test documented finite-window/session behavior. No authentication, persistent replay protection, entropy-failure or DATA guarantee added. |

Dependency findings above come from the installed source, not physical timing measurements: Arduino-ESP32 3.3.11 `cores/esp32/HWCDC.cpp` / `libraries/Preferences/src/Preferences.cpp`; RadioLib 7.8.1 `src/Module.cpp` / `src/BuildOpt.h`. No external library upgrade is needed.

## Proposed minimal seam — recorded before implementation

1. Move RadioEvent values into hardware-independent `common/radio/radio_transport.h` and define only the six operations apps already use: receive, transmit, transmitting, receiving, healthy and timeOnAir.
2. RadioDriver implements this single interface and retains sole ownership of Module/LLCC68/SPI, begin/stop/poll and ISR handling. No factories, task framework or heap-owned interface objects.
3. GatewayApp and ClientApp accept a reference to that interface; SerialLogger references the hardware-independent event type. State logic and firmware timing policy remain unchanged initially.
4. Native-only Arduino/esp_timer compatibility headers provide deterministic time and captured serial writes. Compile the actual production GatewayApp, ClientApp, codec, tracker, config and SerialLogger .cpp files. Fake transport models one TX/RX operation and explicit packet delivery/drop; it does not emulate LLCC68, RF propagation, airtime, hardware CRC or NVS.
5. Replays/redeliveries use the exact captured bytes and assert unchanged network/source/boot/sequence/payload. The firmware still issues a new identity after a failed transaction; no automatic retry or later-milestone MAC is introduced.

The seam is authorized by the software verification task; this document and the accompanying proposal precede any source refactor. Results, exact scenarios, diffs and remaining limitations will be appended after execution.

## Executed results — iteration 007

Firmware stays **0.2.0-alpha.1 / protocol 1**. No packet/state defect was reproduced in the scenarios below. The only reproduced production-source portability defect was ERR-011, fixed by replacing `<inttypes.h>` with C++ `<cinttypes>` in ClientApp and SerialLogger; formats and state statements are unchanged.

Exact commands (workspace root; reviewed execution outside the restricted sandbox):

```powershell
$env:PYTHONUTF8 = '1'
& <PIO_PYTHON> -m platformio run -e esp32c3_supermini
& <PIO_PYTHON> -m platformio test -e native
```

| Check | Actual result | Evidence |
|---|---|---|
| ESP32 compilation | Exit 0, PASS, 15.89 seconds | `logs/build/iteration-007-esp32.log` |
| All native suites | Exit 0, **92 passed / 0 failed / 0 skipped**, 24.35 seconds | `logs/test/iteration-007-native.log` |
| Production app scenarios | 38 PASS | Same native log, test_app_integration |
| Codec / config / console / sequence / logger | 18 / 13 / 10 / 7 / 6 PASS | Same native log |
| Source-preservation checks | PASS: only expected constructor/header changes in app bodies; protected implementations unchanged | `logs/build/iteration-007-static-review.log` |
| Ordinary Git comparison | Not available: no Git repository, rev-parse exit 128 | `logs/build/iteration-007-git-status.log` |
| Physical NVS, radio or RF | NOT RUN | No physical evidence claimed |

Toolchain remains Core 6.1.19, installed pioarduino/espressif32 55.03.311, Arduino-ESP32 3.3.11, RISC-V GCC 14.2.0, RadioLib 7.8.1; native 1.2.1 / Unity 2.6.1 / MinGW GCC 6.3.0. Native code builds with `-Wall -Wextra -Werror`. The ESP32 build retains RadioLib's known USB CDC warning (ERR-005).

Build-reported RAM: 24,516 bytes; flash: 363,466 bytes. Application image SHA-256: `1eb8c7fafeb36e87035095a9c357a9f024e8164c88e43a88274831c9618a34da`. Exact source/test/config hashes: `logs/build/iteration-007-source-manifest.sha256`. BUILD_ID remains the earlier candidate label; use these hashes for the exact snapshot.

## Exact scenario coverage and interpretation

All 92 executed source-case names and PASS results are enumerated in `logs/test/iteration-007-scenarios.md`. These are test/scenario counts; instrumented line/branch coverage percentages were **NOT MEASURED**.

| Production scenario group | Assertions exercised |
|---|---|
| Happy path / repeated operation | Correct HELLO/ACK through both actual apps; 200 complete repeated transactions; no duplicate acceptance or timeout |
| Full sequence exhaustion | Separate 65,537 transactions; sequences 0–65535 then 0 under a fresh boot ID for both client and gateway; echoed identity remains correct |
| Loss | Dropped HELLO; dropped ACK; timeout; no automatic retry; the next new transaction consumes a new sequence |
| Delay / stale ACK | Acceptance one microsecond before deadline, rejection exactly at and after deadline, old ACK during a new outstanding transaction, duplicate ACK after success |
| Wrong identity/address/type | Echoed source/boot/sequence, gateway sender, network, destination; wrong role message type |
| Malformed inputs | 44 HELLO input variants (all 28 short-header lengths plus field/length/trailing/session errors); 45 ACK variants (37 truncations plus eight mutations); later valid packet still succeeds |
| Duplicate/replay | Byte-identical empty and 68-byte payload replay; same logical identity/bytes; gateway accepts once, re-ACKs duplicate with correct original echo |
| Driver-facing failures | Client/gateway TX-start errors and asynchronous TX errors; RX-start recovery spacing and three-failure limit; unavailable transport blocks new HELLO |
| Receive errors | CRC/header-category, RX-timeout and generic transport error events do not extend the outstanding ACK deadline |
| Session lifecycle | New ClientApp/GatewayApp objects emulate reboot; old client ACK rejected, new session starts at sequence 0, gateway restart loses its documented volatile history, retained old-session duplicate echoes original identity |
| Ordering/window | Within-window out-of-order packet accepted once; duplicate suppressed; sequence older than the 64-entry window rejected without ACK |
| Clock boundaries | ACK wait and receive recovery across 32-bit microsecond wrap; maximum interval at an absolute 64-bit time beyond that wrap |
| Controlled-stop gates | No new client TX or gateway ACK with allowNew=false; active client TX completion/expiry still processed. This tests app gates, not Runtime's reboot dispatcher. |
| Diagnostics/config | Real logger FIFO/overflow/backpressure/short/zero writes/N/A formatting; valid-integrity incompatible schema/role/IDs/reserved-byte fixtures rejected by real config decoder |

The fake endpoint has one operation and one 96-byte packet capture; tests explicitly deliver or drop bytes only while RX is enabled and assert no transport-contract violation. Fake airtime is a synthetic constant (1000 us), errors are synthetic codes and the two endpoint clocks share virtual time. No RF model, real waiting, physical link margin, calibrated CRC count, PDR, latency or multi-node capacity is inferred from these tests. Object recreation does not execute an ESP32 reset or prove entropy.

## Changes and preservation review

Production changes are deliberately small: new 30-line `src/common/radio/radio_transport.h`; driver header implements the same six operations; app headers/constructors take RadioTransport references; logger header takes the independent event type; Runtime header directly includes its concrete driver; client/logger use `<cinttypes>`. No state-machine statement was changed, and no runtime interface allocation is added.

Static comparison verifies `src/main.cpp` (15 lines), FirmwareRuntime.cpp, NvsConfigStore.cpp, RadioDriver.cpp, FrameCodec.cpp, DeviceConfig.cpp, SequenceTracker.cpp, pins, PHY, VERSION and PROTOCOL_VERSION are byte-identical to the pre-edit snapshot. App statement bodies and logger implementation are also identical after the expected signature/include substitutions. All LLCC68/SPI operations remain in the concrete RadioDriver implementation; native-only headers are not added to the firmware include path.

Test additions: `test/test_app_integration/test_main.cpp` executes the real roles; `test/test_serial_logger/test_main.cpp` executes the real queue; `test/support/fake_radio_transport.h` captures bounded packets; `test/support/platform/Arduino.h` and `esp_timer.h` adapt native serial/time only; `test/test_role_config/test_main.cpp` adds four semantic blob cases. `platformio.ini` expands only the native source/include filters. Inventory corrections and audit/evidence procedures are aligned in English and Chinese.

Complete source/document file inventory: `logs/build/iteration-007-changed-files.txt`. Complete Git no-index patch: `logs/build/iteration-007-complete.diff`, comparing `iteration-006-baseline` to `iteration-007-review-snapshot`. Exit 1 means differences were found, not a failed build. New raw evidence files and snapshots live under logs and are listed separately from project changes; no Git repository or commit was created.

## Verified, simulated and still unverified

- **Verified software:** executed production app/codec/tracker/config/console/logger behavior for the listed host inputs, plus ESP32 compile compatibility and preserved source-policy comparison.
- **Simulated conditions:** transport delivery/loss/duplicates/delay/error events, object recreation, virtual time and Serial byte capacity. These stimulate actual production state machines; they do not replace those machines with a model.
- **Unverified physical/dependency behavior:** actual Runtime boot/reboot dispatch, esp_random entropy/fault liveness, Preferences/NVS durability and power loss, driver poll/IRQ/SPI/BUSY/CRC, timing under flash/USB blocking, supply/pins/antenna/RF/regulatory settings. ERR-005/008/009/010 and the audit race/random assumptions remain open.

M0–M2 physical acceptance remains IN PROGRESS. No automatic retries, TDMA, ACK bitmap, selective retry scheduling, ADR or excluded integration was added. Next software work can target driver-API fault injection and config-store boundaries; physical work waits for procurement, fabrication and assembly.
