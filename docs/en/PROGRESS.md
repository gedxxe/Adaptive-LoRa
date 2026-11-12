# Development Progress

## Iteration013 physical-readiness package verified; implementation published

2026-11-13 client date. Direct baseline9341e7c0016c66fd9316d489ccfca00a646ae2cb, branch iteration-013-bench-readiness; main5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861 not used. ESP32 exit0 PASS13.053s; native exit0 **290/290 PASS**61.980s, all cases/source unchanged. Capture utility13/13 fake-port tests exit0; separate from native and physical counts. No source/bin/native test/pin/PHY/watchdog/library changes; RAM24,564/flash364,308 unchanged, main15 lines. Full diff/security/history checks passed; implementation `1343e45ae642d0de7c4015ae19575e4e005f1646` committed/pushed and independently matched remote013 (exit0). main/012 unchanged. Receipt `logs/build/iteration-013-publication-receipt.json` is recorded in a documentation-only follow-up. ERR-024 initial approval rejection resolved by rechecking explicit Stage8 authorization; no merge/PR/tag.

PHYSICAL_ACCEPTANCE_READINESS.md links the backed-up single-MCU procedure, pair gates/instruments, ERR-021 disposition and capture/CSV spec. Module pin functions cross-checked against current official V1.1.0; proposed GPIOs/actual boards/wiring remain unverified. Only read-only enumeration (no ports); no open/upload/erase/reset/RF. Capture interrupted-success ERR-022 fixed; SDK startup may format NVS (ERR-023 source risk), so backups precede app boot. ERR-021 holds unattended/stuck-BUSY recovery acceptance; no assumed default watchdog reset. Advanced replay/negative-identity/corrupt-NVS fixtures remain unprovided/unrun.

M0/M1/M2 physical status IN PROGRESS/NOT RUN, M3–M8 NOT STARTED. Next: identify a connected board and execute the protected MCU-only procedure if authorized/backups verified; procure secondMCU/RF termination/antenna and instruments, verify wiring/supply/USB/RF authorization, prepare controlled fixtures and resolve recovery holds before full pair acceptance. No further speculative abstractions/tests planned.

## Historical Iteration 012 software verification PASS; implementation published

Direct baseline8fa0a473b6cc0905ddff53e4b9a5f22806e058e2, branch iteration-012-runtime-reliability; main5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861 unchanged at preflight. `pio test -e native` exit0 **290/290 PASS**,77.575s (266 unchanged+24 reliability). `pio run -e esp32c3_supermini` exit0 PASS31.748s. Corrected ERR-019 boundedID/zero sentinel/preserved prior session/1s retry/TX suppression; ERR-020 post-poll app time/post-stop100ms/post-drain reset check. First RED9FAIL/1PASS and focused10/10 records preserved. ERR-018 launcher/GDB corrections recorded. Complete staged review passed; implementation `bdab1cec5de6e38f84b524a8189945dec1ee7dd2` committed/pushed and independently matched remote012 (exit0). main/011 unchanged. Receipt `logs/build/iteration-012-publication-receipt.json` records verified implementation; a documentation-only follow-up preserves it. No merge/PR/tag.

RAM24,564 (+16), flash364,308 (+466). Firmware0.2.0-alpha.1/protocol1/RadioLib7.8.1/NVS/wire/pins/PHY/commands/main15 unchanged; intentional fault/deadline fixes documented in ADR-015. No new compiler warning. DEPENDENCY_BUDGET_AUDIT.md and iteration-012 artifacts contain exact source/ELF/allocation/timing evidence. ERR-021 upstream TX BUSY-ramp loop has no explicit timeout (source risk, not physical reproduction); ERR-005/008/009/010 remain open. No hard latency/entropy/uniqueness/physical claim.

Software finite-value and delayed-boundary behavior is verified; clock/serial/raw RNG/backend faults/reset are simulated. Physical NVS/reset/USB/SPI/IRQ/RF/heap/stack/entropy **NOT VERIFIED/NOT MEASURED**. M0–M2 physical gates IN PROGRESS, M3–M8 NOT STARTED. Next software scope: separately review bounded upstream BUSY-launch options and SDK allocation-failure handling; procurement/assembly required before physical measurement plan.

## Historical Iteration 011 software verification PASS; implementation published

ESP32 final build `pio run -e esp32c3_supermini` exit0, PASS (143.01s); `pio test -e native` exit0, **266/266 PASS** (36.68s): 47 actual runtime cases plus all219 unchanged regressions. Direct baseline `3902de90058ec775587105c5c3edb4914853a9d8`; dedicated branch `iteration-011-runtime-verification`, main is external merge `5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861`. Implementation commit `04280ed3d36e97924a7644c47b1e9cea09b438fd` was pushed and independently matched remote011 (commit/push/ls-remote each exit0). main and010 remain unchanged. Receipt: `logs/build/iteration-011-publication-receipt.json`; a documentation-only follow-up records this verified implementation SHA. No merge, PR or tag.

Firmware0.2.0-alpha.1/protocol1/RadioLib7.8.1 and wire/NVS/pins/PHY/app policy preserved. Minimal reference seam plus Esp32Firmware composition; main15 lines. RAM24,548 (+16), flash363,842 (-12) bytes. Initial native assertion configuration failure ERR-017 is retained and CLOSED after full-width Unity correction; ERR-016 runner workaround verified, root cause unresolved. No new warning; ERR-005/008/009/010 remain. Details: RUNTIME_AUDIT.md and `logs/test/iteration-011-scenarios.md`.

Verified coordinator ordering/dispatch/deadlines and budgets use simulated storage/radio/serial/clock/reset/raw RNG; physical NVS/USB/reset/SPI/IRQ/RF/entropy NOT VERIFIED. Stop failure still permits reset; logs may remain queued; permanent bad RNG may loop indefinitely. M0–M2 IN PROGRESS for physical gates, M3–M8 NOT STARTED. Next software task: dependency timing/allocation and boot-ID failure-liveness assessment. Procurement/assembly and all physical measurements remain pending.

Historical Iteration 010 software checks PASS: ESP32 build exit 0 (146.83 s), native **219/219 PASS**, exit 0 (28.62 s): 56 new actual NvsConfigStore cases and all 163 unchanged regressions. Baseline remote Iteration 009 is `3176e766d3876c86253e7694a7694560be72d1bf`; main at preflight is `2a7a68120552a8037d796cf61b0af11e91abd0f2` after its external merge. Dedicated branch `iteration-010-config-store-verification` published; implementation commit `01370ac3a6fcc04e9b2bd626a46c9c1b95eb73d4` independently matched remote010 (push/verification exit 0). Original remote009 remains `3176e766d3876c86253e7694a7694560be72d1bf`; main remains `2a7a68120552a8037d796cf61b0af11e91abd0f2`. Receipt `logs/build/iteration-010-publication-receipt.json` is recorded in a documentation-only follow-up commit; no merge or PR. Findings/results: CONFIG_STORE_AUDIT.md and `logs/test/iteration-010-scenarios.md`. Physical validation remains NOT VERIFIED.

Historical Iteration 009 software checks PASS: ESP32 build exit 0 (147.15 s), native **163/163 PASS**, exit 0 (26.04 s): 71 new production RadioDriver fault cases plus all 92 existing regressions. Pre-edit findings/coverage are in RADIO_DRIVER_AUDIT.md; exact individual results are in `logs/test/iteration-009-scenarios.md`. Dedicated branch `iteration-009-radio-driver-verification` published; implementation commit `4055a947bdfcd518cdff36d6ae76acf3fabeed93` independently matched remote branch (push/verification exit 0). Publication receipt is `logs/build/iteration-009-publication-receipt.json`, recorded in a subsequent documentation-only commit. Main remains `b040ef111b82db96ab81a3c7377efbcf46385221`; no merge or PR. Physical validation remains NOT VERIFIED.

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
| M2  | Fixed-PHY HELLO/ACK                                            | IN PROGRESS | 219 native tests including production apps/driver/store pass; RF evidence pending |
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

Next physical action: use the Iteration013 backed-up MCU-only procedure once a board is identified; complete procurement/instruments and actual M0 checks. ERR-021 and controlled replay/NVS fixture gates must be resolved before full M1/M2 closure; native/software evidence is already verified. Physical gates wait for the second MCU, antenna and assembly; then follow BENCH_RUNBOOK.md for M0/M1 and >=200 actual M2 HELLO attempts. Do not start M3 until M2 acceptance evidence exists. TDMA/bitmap/retry scheduling/ADR remain unimplemented.

Final evidence-reader check: Windows default decoding failed on a UTF-8 build log; explicit UTF-8 succeeded (ERR-006 recurrence). Firmware/native PASS evidence is unchanged; no new firmware error.

Iteration 009 preservation: RadioDriver state/deadline/error policy remains equivalent after hardware forwarding/constant substitutions; 34 protected original files unchanged. Runtime owns one Llcc68Hardware plus a reference-injected RadioDriver; main.cpp remains 15 lines. The new seam has no explicit heap allocation. RAM 24,532 / 327,680 bytes; flash 363,730 / 1,310,720. Actual adapter/SPI/BUSY/ISR races, Runtime/entropy/NVS/USB and RF remain outside native evidence; M0–M2 overall IN PROGRESS, M3–M8 NOT STARTED.

Iteration 009 review note: staged log display whitespace triggered ERR-015 (check exit 2); log-format normalization is verified closed by staged check exit 0; source/build/test evidence unaffected.

Iteration 010 preservation: NvsConfigStore error/count/cleanup/readback logic and numeric namespace are equivalent after reference forwarding; key `device`, schema 1 and 24-byte format unchanged. Runtime header constructs one PreferencesConfigStorage before its store; runtime implementation and 39 protected original files (including all 163 old test cases) are byte-identical. All 48 frozen inputs match. Main remains a 15-line entry point; no explicit allocation introduced. RAM 24,532 bytes unchanged, flash 363,854 (+124). No new warning messages; ERR-005/008/009/010 remain. Actual NVS/flash durability, power interruption, reboot/corruption recovery, wear/retention and physical radio/RF remain unverified; M0–M2 overall IN PROGRESS, M3–M8 NOT STARTED.
