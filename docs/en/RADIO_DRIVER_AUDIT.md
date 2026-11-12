# Iteration 009 — radio-driver fault-path audit

## Pre-edit baseline and scope

Audit completed before source edits on 2026-11-12. Local main and fetched origin/main both point to `b040ef111b82db96ab81a3c7377efbcf46385221`; working tree was clean, origin is `https://github.com/gedxxe/Adaptive-LoRa.git`. Dedicated branch: `iteration-009-radio-driver-verification`; no remote branch existed at preflight. Firmware 0.2.0-alpha.1, protocol 1, RadioLib 7.8.1. Canonical engineering records, driver/transport/runtime, pins/PHY and current native adapters/tests were inspected.

This is software verification. Inventory remains two RA-01SCH/LLCC68 modules and one ESP32-C3 Super Mini; no assembled pair, second MCU and planned 915 MHz Moxon pending. No physical NVS, SPI, interrupt, RF or antenna claim follows.

## Existing behavior and error precedence

| Operation | Baseline transition / result |
|---|---|
| Construction | OFF, initialized false; poll returns NONE |
| begin | SPI begin; LLCC68 begin with fixed profile; explicit header; CRC length 2; install DIO1 callback only after success; IDLE/initialized true on success, FAULT on first error |
| receive | Before initialization or during TX: UNKNOWN without state change. Otherwise clear software IRQ flag, start infinite RX with all default RX flags also routed to DIO1; RX on success, FAULT on error |
| transmit | Uninitialized, already TX, null, <28 or >96 bytes: PACKET_TOO_LONG without state change. Standby precedes TX start; IDLE/RX/FAULT can start after initialization. Success becomes TX; standby/start failure becomes FAULT. Deadline is now + 2,000,000 us even after a failed attempt |
| stop | Before initialization: success without hardware/state change. Otherwise standby; IDLE on success, FAULT on failure; initialized flag retained |
| TX poll | Deadline checked before IRQ, including TX_DONE at expiry; stop success yields TX_TIMEOUT, stop failure takes precedence. No signal/irrelevant IRQ leaves TX. TX_DONE/TIMEOUT call finishTransmit; finish error becomes FAULT; hardware timeout after successful finish reports ERROR/TX_TIMEOUT but state IDLE |
| RX poll | Software IRQ flag or high DIO1 permits flag read; clear software flag. Ignore nonterminal flags. CRC always fails; HEADER_ERR fails only without HEADER_VALID. Freeze with standby before length/read. Standby failure exits FAULT without finishReceive. Timeout takes precedence over CRC/data; CRC takes precedence over data |
| RX payload | Length outside 28..96 is rejected without readData. Successful read alone enables RSSI/SNR/metrics. readData CRC_MISMATCH maps CRC_ERROR, other errors map ERROR. finishReceive runs after timeout/CRC/length/read paths; cleanup failure overrides event code/type and sets FAULT |

A successful read followed by finishReceive failure retains the read bytes/metrics while reporting ERROR and FAULT; this existing diagnostic behavior will be characterized, not silently changed. Initialization is normally once per boot; a later failed begin currently retains a prior initialized=true flag. Fault recovery through receive/stop is allowed after initialization. No automatic retries or new recovery policy will be added.

## Pinned dependency evidence

Inspected installed `.pio/libdeps/esp32c3_supermini/RadioLib/library.json` (7.8.1), `src/modules/LLCC68/LLCC68.cpp`, `src/modules/SX126x/SX126x.cpp`, `SX126x.h`, `SX126x_commands.cpp/.h`, `src/TypeDef.h` and `src/protocols/PhysicalLayer/PhysicalLayer.h`.

- LLCC68 begin forwards the same frequency/BW/SF/CR/sync/power/preamble/XTAL/LDO arguments. Library inheritance from an SX126x base does not change the project's LLCC68 target.
- readData qualifies HEADER_ERR using absence of HEADER_VALID; CRC_ERR is unconditional. It may return RX_TIMEOUT or other SPI/CRC errors; driver preserves its existing classification.
- finishTransmit clears IRQ then standby; finishReceive performs standby, implicit-timeout workaround and IRQ clear; their returned errors propagate.
- getIrqFlags/getPacketLength/metric getters do not expose separate error status. A synthetic getter failure code must not be invented.
- startReceive uses infinite hardware timeout and flags/mask containing RX_DONE, TIMEOUT, CRC_ERR, HEADER_VALID and HEADER_ERR. Raw SX126x IRQ bits differ conceptually from generic API flags even where numeric values match here.
- Synchronous library BUSY waits/allocations and real interrupt races remain outside host verification (ERR-008/009). Deadline checking cannot preempt a blocking dependency.

## Untestable branches and minimal proposed seam

Current native source filter excludes RadioDriver because its header owns Module/LLCC68 and includes Arduino/RadioLib; poll directly samples GPIO, and the ISR uses a process-wide volatile flag. Existing FakeRadioTransport injects completed app events and therefore cannot execute driver branches.

Introduce a single non-owning RadioHardware reference with only the operations already called: begin/header/CRC/IRQ setup, software IRQ clear/sample, standby, RX/TX start/finish, IRQ flags, packet length/read, RSSI/SNR and airtime. Keep RadioTransport unchanged. Move concrete Module/LLCC68/SPI/GPIO/minimal ISR to Llcc68Hardware within the same radio module. Runtime owns one adapter before one injected driver. No heap ownership, factory, task framework, alternate adapter or app-state redesign.

Use named numeric status/raw-IRQ constants at this seam and compile-time assertions against pinned RadioLib in the production adapter. Native fake provides bounded packet storage, programmable operation codes, IRQ flag/level and call-order tracking; it contains no driver transitions. Native target compiles the actual radio_driver.cpp; all branches/priority decisions remain there. IRQ flag/level stimuli characterize single-threaded logic, not real ISR race freedom. Firmware build verifies adapter API and constant compatibility; source review verifies fixed configuration forwarding.

Proposed tests cover every requested A–G fault category, boundary length, precedence, recovery and wrap-safe deadline. Record exact executed names/counts/results after running; no passing result is asserted at this pre-edit checkpoint.

## Executed results and preservation review

`pio run -e esp32c3_supermini`: exit 0, PASS, 147.15 s. `pio test -e native`: exit 0, 163/163 PASS, zero failed/skipped, 26.04 s. Full stdout/stderr: `logs/build/iteration-009-esp32.log` and `logs/test/iteration-009-native.log`; only workstation paths redacted. All 71 newly added driver cases and 92 unchanged regression cases ran. Every executed case/result is listed in `logs/test/iteration-009-scenarios.md`.

| Coverage group | Cases | Explicit expectations |
|---|---:|---|
| Initialization / uninitialized polling | 8 | Ordered begin/header/CRC/IRQ setup, first-error short circuit, no operations before successful init, retry and repeated-begin flag behavior |
| Receive operation | 5 | Pre-init/TX rejection, clear latch before RX, RX/FAULT transitions, restart and fault recovery |
| Transmit operation / airtime | 10 | Null; all lengths 0..27; 97/256/SIZE_MAX; max valid 96 bytes preserved; pre-init/busy; standby/start errors; recovery/preemption; airtime forwarding |
| Poll / IRQ / TX completion | 11 | Idle/no IRQ; low GPIO with latched signal; high GPIO without latch; zero/unrelated flags; finish ordering; TX_DONE, timeout, finish-error precedence and recovery |
| Timing boundaries | 8 | Before/at/after 2,000,000-us deadline; deadline before pending TX_DONE; standby-error precedence; uint32 wrap; unrelated IRQ cannot extend deadline |
| RX events / payload / metrics | 22 | 28/96 bytes; all short and oversize boundaries; timeout/CRC/header qualification/priority; ignored flags; standby/read/finish errors; fresh metrics only after read success; cleanup override and recovery |
| Stop / repeated operation | 7 | No-op before init, IDLE/RX/TX stop, failed-stop FAULT, successful recovery, cancellation of old deadline, 32 repeated RX/TX pairs crossing wrap |

Status values -701/-702 are deliberately synthetic opaque operation errors; no claim they correspond to observed RadioLib failures. Real defined status values and raw IRQ bits are compile-time checked in the production adapter. Assertions cover returned events/codes, public state observations, exact bytes, forbidden/ordered calls and metrics availability; the fake does not implement state transitions. Size loops reject invalid pointers/lengths before hardware copy. No instrumented line/branch percentage, sanitizer or exhaustive concurrency proof is claimed.

No production defect required a behavior correction in these cases. Static normalization against baseline preserves every driver condition, state assignment, deadline and error priority after explicit hardware-call/constant substitutions. Thirty-four protected original files (all other production/header/test files plus versions) are byte-identical, including main.cpp (15 lines), app/runtime implementations, NVS schema/stored bytes, codec, transport, pins and PHY. Firmware environment is unchanged; only native source filter adds radio_driver.cpp. Original Module constructor/SPI/profile/RX flags/ISR/getter/call arguments are retained by the adapter. Evidence: `logs/build/iteration-009-source-review.json` and source hashes `iteration-009-source-manifest.sha256`.

Reference dispatch changes binary layout and may change execution time; binary identity or measured timing equivalence is not claimed. Build RAM 24,532 bytes, flash 363,730 bytes; compared with iteration 008, +16 RAM/+264 flash. The seam adds no explicit heap allocation; RadioLib scratch allocations and synchronous calls remain ERR-008/009/010 limits.

Native tests bypass the real Llcc68Hardware implementation. Adapter compatibility/constants are compiled on ESP32 and call forwarding is statically reviewed, not physically exercised. Actual SPI commands, BUSY/IRQ timing, ISR interleavings/lost-edge assumptions, library getter failures with no exposed status, radio startup/reset, NVS durability/reboot, supply/antenna/RF and regulatory settings remain NOT VERIFIED. Next software task: NVS/configuration-store boundary verification; any driver policy change needs a separately justified defect and regression case. M0–M2 physical gates remain IN PROGRESS, M3–M8 NOT STARTED.
