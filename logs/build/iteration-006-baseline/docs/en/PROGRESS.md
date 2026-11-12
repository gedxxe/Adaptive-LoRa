# Development Progress

This file is the current project snapshot. Detailed iteration history is
append-only in `BUILD_MEMORY.md`; issue history is in `ERROR_LOG.md`.

Status values: `NOT STARTED`, `IN PROGRESS`, `BLOCKED`, `PASS`. A
milestone is `PASS` only when its acceptance criteria and evidence are
recorded.

## Current baseline

| Item                      | Current value                                             |
|---------------------------|-----------------------------------------------------------|
| Firmware architecture     | One image; role loaded from NVS at boot                   |
| Gateway hardware          | ESP32-C3 Super Mini + Ai-Thinker Ra-01SCH (LLCC68), qty 1 |
| Client hardware           | ESP32-C3 Super Mini + Ai-Thinker Ra-01SCH (LLCC68), qty 1 |
| Radio driver              | RadioLib `LLCC68`                                         |
| RadioLib pin              | 7.8.1                                                     |
| Frequency                 | 915.0 MHz provisional bench setting                       |
| Initial data PHY          | SF7 / BW125 / CR4/5                                       |
| Common control PHY        | SF9 / BW125 / CR4/5                                       |
| Initial TX power          | 10 dBm                                                    |
| Antenna                   | 915 MHz Moxon; matching and performance not yet validated |
| Network ID                | 1                                                         |
| Gateway node ID           | 1                                                         |
| Initial client node ID    | 2                                                         |
| Firmware version baseline | 0.2.0-alpha.1 implementation candidate                    |
| Protocol version          | 1                                                         |

## Current status

M0–M2 source is implemented as candidate `0.2.0-alpha.1`. Final native tests: 44/44 PASS, exit 0 (`logs/test/iteration-005-native.log`). Final frozen-source ESP32 build: PASS, exit 0 (`logs/build/iteration-005-esp32.log`). Physical NVS/radio/RF tests: NOT RUN. Only Bluetooth ports were enumerated; no ESP32 port or hardware photographs/schematic/continuity record were available. No milestone is PASS.

| ID  | Milestone                                                      | Status      | Evidence                                 |
|-----|----------------------------------------------------------------|-------------|------------------------------------------|
| M0  | Board, module, pin, power, antenna, and toolchain confirmation | IN PROGRESS | Resolved toolchain recorded; physical checks pending |
| M1  | LLCC68 initialization on both devices                          | IN PROGRESS | Firmware compiles; physical initialization/NVS persistence pending |
| M2  | Fixed-PHY HELLO/ACK                                            | IN PROGRESS | 44 native tests pass; two-device RF evidence pending |
| M3  | Beacon and one-client TDMA                                     | NOT STARTED | Deferred until M2 passes                 |
| M4  | ACK bitmap and selective retry                                 | NOT STARTED | Deferred until M3 passes                 |
| M5  | Versioned ADR SF7/SF8/SF9                                      | NOT STARTED | Deferred until M4 passes                 |
| M6  | Synchronization loss and rejoin                                | NOT STARTED | Deferred                                 |
| M7  | Controlled Moxon RF evaluation                                 | NOT STARTED | No controlled RF data recorded           |
| M8  | 5–70 logical-client host simulation                            | NOT STARTED | No simulation recorded                   |

## Toolchain record

| Item                            | Resolved value                                       |
|---------------------------------|------------------------------------------------------|
| PlatformIO Core                 | 6.1.19                                               |
| PlatformIO espressif32 platform | 55.03.311 / display 55.3.311, installed pioarduino fork |
| Arduino-ESP32 core              | 3.3.11; framework libs 5.5.5+sha.b774170ff46          |
| Compiler                        | RISC-V GCC 14.2.0 (esp-14.2.0_20260121)              |
| RadioLib                        | 7.8.1 pinned and resolved                           |
| Host compiler                   | MinGW GCC 6.3.0                                     |
| Host test framework             | Unity 2.6.1 / PlatformIO native 1.2.1                |

## Open hardware checks

- [ ] Confirm exact ESP32-C3 Super Mini board revision and flash size.
- [ ] Confirm exact Ra-01SCH module marking and pin orientation.
- [ ] Verify all proposed GPIO connections by schematic/continuity.
- [ ] Confirm radio 3.3 V rail and measure it during TX.
- [ ] Confirm antenna connector, feed line, and 50-ohm path.
- [ ] Confirm local frequency/channel/power requirements before
  deployment.
- [x] Record the exact toolchain versions after first successful build.

## Current blockers

Native ERR-002/ERR-003 and firmware ERR-004 are closed with command evidence. USB CDC warning ERR-005 requires physical serial verification. All M0 physical checks, M1 radio/NVS tests and M2 RF acceptance remain open. Environment metadata probe failures are documented as ERR-006 and resolved using UTF-8 output/outside-sandbox tool execution.

ERR-007's header-error filter is corrected, statically aligned with pinned RadioLib and compile-verified in iteration 005; physical IRQ behavior remains an M1/M2 gate. Complete file responsibilities and M3–M8 plan: IMPLEMENTATION_REPORT.md. Final binary/source hashes: iteration-005 build records.

## Next action

Complete M0 physical inspection and permitted test setup using BENCH_RUNBOOK.md, flash the same image onto both identified units, verify missing/invalid/saved configuration, initialize both radios and record >=200 HELLO attempts with both raw serial logs. Do not start M3 until M2 acceptance evidence is complete. M3 then adds one-client beacon/slot timing with measured switching/IRQ/drift and justified guards; M4–M8 remain documentation-only plans.
