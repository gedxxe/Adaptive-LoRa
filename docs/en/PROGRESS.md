# Development Progress

Iteration 009 software checks PASS: ESP32 build exit 0 (147.15 s), native **163/163 PASS**, exit 0 (26.04 s): 71 new production RadioDriver fault cases plus all 92 existing regressions. Pre-edit findings/coverage are in RADIO_DRIVER_AUDIT.md; exact individual results are in `logs/test/iteration-009-scenarios.md`. Dedicated branch `iteration-009-radio-driver-verification`; commit/push pending final review at this checkpoint. No main merge or PR. Physical validation remains NOT VERIFIED.

Project: **Adaptive-LoRa**. Research title: Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks.

Historical iteration 008 established the independent repository. Identity/path labels in earlier publication copies were neutralized with provenance; chronology, errors and original outcomes remain. Its validation: ESP32 build PASS, exit 0 (`logs/build/iteration-008-esp32.log`), native **92/92 PASS**, exit 0 (`logs/test/iteration-008-native.log`), including 38 integration scenarios. The four-boundary architecture/gaps are documented, not newly implemented. Firmware 0.2.0-alpha.1, protocol 1, PHY, packet logic and legacy storage bytes are unchanged.

Git publication is verified: `main` was pushed to the authorized public Adaptive-LoRa repository; establishment commit `1d7ab54379221dff8b4eb9347833282657b6e1dc` matches remote main (`git ls-remote`, exit 0). The initial remote was empty, so no history merge was needed. Staged review/security checks passed. The subsequent documentation-only receipt records the already verified establishment SHA in BUILD_MEMORY.md and `logs/build/iteration-008-publication-receipt.json`. No physical validation is claimed.

This file is the current project snapshot. Detailed iteration history is
append-only in `BUILD_MEMORY.md`; issue history is in `ERROR_LOG.md`.

Status values: `NOT STARTED`, `IN PROGRESS`, `BLOCKED`, `PASS`. A
milestone is `PASS` only when its acceptance criteria and evidence are
recorded.

## Current baseline

| Item                      | Current value                                             |
|---------------------------|-----------------------------------------------------------|
| Firmware architecture     | One image; role loaded from NVS at boot                   |
| MCU inventory             | 1 ESP32-C3 Super Mini; second not purchased; pair not assembled |
| Radio inventory           | 2 Ai-Thinker RA-01SCH/LLCC68 modules; pair not assembled |
| Radio driver              | RadioLib `LLCC68`                                         |
| RadioLib pin              | 7.8.1                                                     |
| Frequency                 | 915.0 MHz provisional bench setting                       |
| Initial data PHY          | SF7 / BW125 / CR4/5                                       |
| Common control PHY        | SF9 / BW125 / CR4/5                                       |
| Initial TX power          | 10 dBm                                                    |
| Antenna                   | 915 MHz Moxon planned, not yet fabricated               |
| Network ID                | 1                                                         |
| Gateway node ID           | 1                                                         |
| Initial client node ID    | 2                                                         |
| Firmware version baseline | 0.2.0-alpha.1 implementation candidate                    |
| Protocol version          | 1                                                         |

## Current status

Iteration-007 baseline evidence is preserved: candidate `0.2.0-alpha.1` / protocol 1, compile PASS and 92/92 host cases. Iteration 008 re-executed those checks after identity changes: 38 production-app scenarios, codec 18, role/config 13, console 10, sequence/timing 7, logger 6. These are scenario counts, not line/branch coverage percentages.

Corrected stock: two LLCC68 modules, one MCU, second MCU not purchased, Moxon not fabricated; no assembled pair. Physical NVS/radio/RF tests: NOT RUN. The actual app statement bodies are unchanged; only the transport constructor type and standard C++ format-header includes differ. Main remains unchanged at 15 lines. Audit/proposal/results: SOFTWARE_AUDIT.md; exact cases: `logs/test/iteration-007-scenarios.md`. No complete milestone is PASS.

| ID  | Milestone                                                      | Status      | Evidence                                 |
|-----|----------------------------------------------------------------|-------------|------------------------------------------|
| M0  | Board, module, pin, power, antenna, and toolchain confirmation | IN PROGRESS | Resolved toolchain recorded; physical checks pending |
| M1  | LLCC68 initialization on both devices                          | IN PROGRESS | Firmware compiles; physical initialization/NVS persistence pending |
| M2  | Fixed-PHY HELLO/ACK                                            | IN PROGRESS | 163 native tests including production apps/driver pass; RF evidence pending |
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

ERR-011 (legacy MinGW format macros) is fixed by C++ `<cinttypes>` and verified by 92 passing native cases/ESP32 compile. No application packet/state defect was reproduced in those scenarios. ERR-008 synchronous NVS/BUSY, ERR-009 dependency scratch allocations, ERR-010 SDK serial waits and ERR-005 physical CDC behavior remain open limitations; hardware IRQ/random-source assumptions are also unverified. Native driver fault tests cover policy around returned operations, not the actual dependency timing/allocation or ISR interleavings.

ERR-007's header filter remains unchanged from iteration 005; physical IRQ behavior is still an M1/M2 gate. Migration changes are mechanical namespace/macro labels and a byte-equivalent storage namespace representation; behavior equivalence is checked across 35 source/header/test files. New source/binary fingerprints are in iteration-008 records; older hashes identify original artifacts. Git was initialized only after the pre-edit identity audit. No new scheduler, bitmap, selective retry or ADR code exists.

## Next action

Next software task: configuration-store/NVS boundary verification with a minimal persistence fake, preserving the schema/key/blob and boot policy. Driver policy fault verification is complete for the documented 71 cases; resolve dependency timing/memory budgets before claiming hard guarantees. Physical gates wait for the second MCU, antenna and assembly; then follow BENCH_RUNBOOK.md for M0/M1 and >=200 actual M2 HELLO attempts. Do not start M3 until M2 acceptance evidence exists. TDMA/bitmap/retry scheduling/ADR remain unimplemented.

Final evidence-reader check: Windows default decoding failed on a UTF-8 build log; explicit UTF-8 succeeded (ERR-006 recurrence). Firmware/native PASS evidence is unchanged; no new firmware error.

Iteration 009 preservation: RadioDriver state/deadline/error policy remains equivalent after hardware forwarding/constant substitutions; 34 protected original files unchanged. Runtime owns one Llcc68Hardware plus a reference-injected RadioDriver; main.cpp remains 15 lines. The new seam has no explicit heap allocation. RAM 24,532 / 327,680 bytes; flash 363,730 / 1,310,720. Actual adapter/SPI/BUSY/ISR races, Runtime/entropy/NVS/USB and RF remain outside native evidence; M0–M2 overall IN PROGRESS, M3–M8 NOT STARTED.

Iteration 009 review note: staged log display whitespace triggered ERR-015 (check exit 2); log-format normalization is verified closed by staged check exit 0; source/build/test evidence unaffected.
