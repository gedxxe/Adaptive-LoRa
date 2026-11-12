# Iteration 011 - firmware runtime lifecycle audit

## Pre-edit baseline

Recorded 2026-11-12 18:35 WIB, before source changes. Local and fetched origin/iteration-010-config-store-verification exactly match `3902de90058ec775587105c5c3edb4914853a9d8`; working tree clean. Origin is the authorized Adaptive-LoRa repository. Remote main is `5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861`, an external merge of Iteration 010 with the same source tree. Target iteration-011-runtime-verification was absent; created directly from the specified remote010 HEAD, not main. Firmware 0.2.0-alpha.1, protocol 1, RadioLib 7.8.1; baseline 219 native cases.

Read current instructions, canonical architecture/audit/storage audit/progress/build/error/decision/test records and production runtime, console, store/adapters, radio driver/interfaces, role apps, logger, main and existing fakes. Physical inventory remains two LLCC68 modules/one MCU/no fabricated Moxon/no assembled pair.

## Execution order and existing policy

| Path | Baseline behavior |
|---|---|
| setup | Serial.begin(115200), actual store.load into fresh default UNSET config, logger/console begin, BOOT and CONFIG validity logs |
| invalid load | PROVISIONING/radio_disabled and status; early return before radio.begin, app emplace or boot-ID generation |
| valid load | RadioDriver.begin first, RADIO_INIT log; only on success emplace exactly gateway or client and call app.begin(micros); then status |
| gateway activation | Generator initializes session, receive starts immediately; RX-start failure is app-active but RADIO_FAULT, with existing bounded recovery policy |
| client activation | Generator initializes session, first HELLO scheduled at absolute esp_timer time +1,000,000 us; first loop normally starts RX |
| loop | Capture uint32 micros once; driver.poll; selected app.update with allowNew=!rebootPending; consume <=64 Serial bytes through actual console; handle pending stop; drain logger once; test reset deadline |
| commands | help/status/show/save/reboot and parser errors exactly as existing console. show is staged, status loaded. save validates/persists/readbacks staged data; loaded role/IDs/interval stay unchanged until a new boot |
| reboot | Command sets pending and logs; wait only while driver transmitting. Then stop exactly once, log success or standby_failed_restarting, mark stopped, deadline=captured now+100,000 us. Stop failure does not prevent reset |
| reset | After one normal log drain, request ESP.restart when wrap-safe deadline reached; there is no queue-empty/USB-flush gate |

Apps update before consuming this loop's serial input. A due transaction may start before a same-loop reboot command is parsed; after pending is set, subsequent app updates block new transactions. Existing completion/error handling still executes while pending. Repeated reboot commands do not reschedule an already armed deadline. Serial commands remain serviceable while waiting for TX/deadline. The reset fake may return, unlike real reset; tests stop the simulated instance at the first observed reset.

TX deadline and errors are actual RadioDriver policy, not runtime timers. A TX deadline can call standby in poll, followed by runtime's own final stop in that same loop. Uninitialized/OFF/FAULT poll returns NONE without sampling hardware. stop before initialization is also no-op. These must be exercised through the actual driver, not a modeled lifecycle.

## Hardware/resource and diagnostic boundaries

Global hardware objects are constructed even when config will be invalid. Inspected installed RadioLib 7.8.1 Module.cpp and hal/Arduino/ArduinoHal.cpp: the existing Module constructor allocates ArduinoHal; constructors store fields, while SPI/GPIO initialization occurs later through init/begin. Do not claim allocation-free dependency construction or that host tests execute those constructors. No new allocation is intended. Invalid-config poll/stop can be tested for zero RadioHardware calls; real constructor/SDK behavior remains separately inspected and physically unverified.

Logger has 16 fixed 512-byte entries and one <=128-byte drain per loop. Backpressure/short writes can leave diagnostics pending at reset; that is an existing policy, not a reliable USB-flush guarantee. status exposes drop count. Serial/flash/BUSY calls are synchronous (ERR-008/010) and captured loop time is not re-sampled after them. No hard deadline/latency guarantee is inferred.

freshBootId repeatedly calls esp_random until nonzero and different from previous. Apps receive this function pointer at activation and on sequence exhaustion. A permanently failing/repeating random source can keep the loop unbounded; no liveness fix or entropy claim is included. Across a real new boot previous=0, so prior-session uniqueness is probabilistic rather than persisted. Tests will exercise explicit successful rejection sequences, not hang on an infinite bad source.

## Minimal proposed seam

1. FirmwareRuntime constructor accepts existing ConfigStorage and RadioHardware references. It retains actual NvsConfigStore, RadioDriver, logger, console and inline optional production apps, plus all existing coordinator policy.
2. Move only PreferencesConfigStorage/Llcc68Hardware construction to a single ESP32 composition object whose members outlive runtime. Main stays a thin setup/loop delegate. No new radio/store lifecycle interface, platform class hierarchy, factory or task.
3. Extend the existing native-only Arduino/esp_timer approach: bounded serial input with begin/read observation, micros from the same virtual clock, and native esp_system/esp_random shims for reset/reason and scripted raw random values. Actual freshBootId, console, logger, apps, driver and store execute. A default-null begin observer in the existing hardware fake checks cross-component init order.
4. Installed native GCC 6.3 has experimental/optional only. Select std::optional when available (firmware), otherwise a project namespace alias to std::experimental for native; no additions to namespace std or duplicated coordinator. Use C++ cinttypes in runtime as already required by legacy-host format macros in iteration 007; log fields unchanged.

Native scenarios cover required A-G: valid/invalid boot and exclusive roles, real storage+serial provisioning and staged activation/new-instance reboot, input budget/fragmentation/errors, actual driver events/recovery, stop/reset timing/wrap/failure/log backpressure, deterministic initial/regenerated sessions. No passing result asserted before execution. Potential policy concerns above will be reproduced/characterized before considering a change; no behavior fix is proposed at this checkpoint.

## Executed verification and scenario coverage

Final ESP32 command `pio run -e esp32c3_supermini`: exit 0, PASS, 143.01 s (runner 143.563 s). Corrected `pio test -e native`: exit 0, **266/266 PASS**, 36.68 s (runner 37.234 s). All eight original suites (219 cases) are byte-identical and their ordered names/results match Iteration 010. The new suite executes 47 cases against actual FirmwareRuntime, NvsConfigStore, RadioDriver, apps, console, codec and logger. Individual names/results: `logs/test/iteration-011-scenarios.md`. No line/branch coverage percentage is claimed.

| Required group | Primary runtime cases | Assertions and stimuli |
|---|---:|---|
| A Boot/selection | 11 | Gateway/Client exclusivity; serial before closed storage/read before radio before raw RNG/app; missing/corrupt/schema/role/IDs/open/short-read failures; init/header/CRC failures suppress app/RNG |
| B Provisioning | 8 | Actual staged Gateway/Client edits and saved bytes, invalid save without access, write/readback/mismatch errors, loaded role/IDs preserved until new coordinator instance |
| C Serial | 7 | 115200, exactly 64 consumed bytes per loop, fragments, multiple commands, CR/LF and single save, invalid numbers/commands, control-byte and overflow recovery |
| D Radio dispatch | 4 | Gateway HELLO to ACK, Client matching ACK acceptance, RX-start fault/recovery, CRC error dispatch; invalid-config OFF poll/stop access assertions also in A/E |
| E Reboot | 10 | Idle/RX/TX, actual TX done and driver timeout, one stop, failed stop still resets, repeated command, blocked transactions, app-before-input order, before/at/after and uint32 wrap deadline |
| F Session | 3 | Actual raw RNG zero rejection, gateway sequence exhaustion with zero/previous-ID rejection, saved-role simulated reboot/new session |
| G Diagnostics/resources | 4 | Unavailable/one-byte output at reset, queue drops/status, boot/reset-reason/version/unset metrics; other groups assert actual error and event output |

F runs 65,537 HELLO/ACK transactions through actual coordinator/driver/gateway. It asserts sequence wrap creates boot ID 200 after rejecting zero and previous 100, without reinitializing hardware or reopening boot storage. Initial Client identity and both role-generator bindings are checked elsewhere. These are scripted sessions, not entropy measurements or cross-boot uniqueness guarantees.

Initial ESP32 build also passed (144.12 s). Initial native run retained 219 passing regressions but failed to compile runtime tests because installed 32-bit host Unity did not enable full-width assertions (ERR-017). Native-only UNITY_SUPPORT_64 fixed the configuration; all 266 then passed without removing/truncating assertions. The failed log and retry remain separate. ERR-016 records an earlier restricted-runner stall and successful host-shell workaround, without attributing an unsupported root cause.

## Final compatibility, ownership and resources

Implemented the proposed reference seam and ESP32 composition, with no runtime policy correction. Runtime cpp differs only in cinttypes include; setup/loop/commands/status/freshBootId bodies are unchanged. Existing RadioHardware/ConfigStorage/RadioTransport interfaces and app/store/driver/console policy are unchanged. Concrete adapter headers only clarify their owner in comments. Esp32Firmware owns storage then radio then coordinator; reverse destruction keeps referenced adapters alive until the coordinator is destroyed. Firmware object lifetime is static; tests use in-place optional emplacement and fake reset observation, not physical reset/reinitialization of the adapter. setup remains a once-per-instance contract.

Final static RAM 24,548/327,680 bytes (**+16** versus Iteration 010); flash 363,842/1,310,720 (**-12**). Size deltas are compiler reports, not an explanation of individual alignment/linker contributions. Full source fingerprints (52 inputs), firmware-bin SHA-256, byte-preservation proof and command results are in `logs/build/iteration-011-source-manifest.sha256`, `iteration-011-source-review.json` and `iteration-011-validation.json`. Native-pass manifest differs from final only in two concrete-adapter owner comments excluded from native compilation; final ESP32 ran after both comments and the native flag change.

No explicit production heap allocation, dynamic factory, extra task, second radio or second coordinator is introduced. The coordinator retains the existing inline optional apps, fixed frames and logger queue. Local runtime arrays (320-byte status/config lines) and call structure are unchanged; stack high-water, peak heap, construction latency and worst-case dependency timings are **NOT MEASURED**. Standard optional is used in firmware and legacy experimental optional on the host. Host-only serial input is bounded at4096 bytes; random script at16 values with exhaustion assertions; low-level traces are bounded/reset between calls. Captured host output and test convenience strings may allocate; they are not firmware hot-path guarantees.

Versions, numeric NVS namespace/device key/schema/24-byte blob, wire identity/layout, pins/PHY, error classes, log meanings and all219 prior cases remain protected. Only the existing RadioLib USB CDC warning remains (ERR-005, two occurrences); native has no warning. Resolved Core6.1.19, platform55.03.311 (display55.3.311), Arduino3.3.11/IDF libs5.5.5+sha.b774170ff46, RISC-V14.2.0+20260121, RadioLib7.8.1; native1.2.1, Unity2.6.1, MinGW6.3.0. No dependency upgrade.

## Limits and next work

Verified software: coordinator ordering, guard conditions, actual dispatch/state handling, format/error compatibility, deterministic resource budgets and tested deadlines. Simulated: backend failures/IRQ inputs, raw RNG, clock, serial capacity, instance recreation and reset requests. Physical NVS/commit/retention, actual reset, USB/Serial locks, SPI/BUSY, GPIO/IRQ races, RF, antenna and entropy quality remain **NOT VERIFIED**. No assembled pair, no upload or radio transmission was performed.

Open software assumptions include synchronous storage/BUSY/serial calls (ERR-008/010), RadioLib allocation (ERR-009), captured-time staleness and the unbounded freshBootId rejection loop for a permanently bad source. No unsafe memory or coordinator-state defect was reproduced by these tests; this does not prove absence of races or untested failures. No later MAC/application integration was added. M0–M2 physical gates remain IN PROGRESS; M3–M8 NOT STARTED. Recommended next software task: focused dependency timing/allocation and boot-ID failure-liveness assessment, with explicit budgets and no policy change without reproduction/approval of the concrete correction.
