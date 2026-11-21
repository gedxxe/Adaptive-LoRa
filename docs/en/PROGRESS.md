# Development progress

<!-- current-state:start -->
## Current state — Iteration 021

Iteration 021 is on `main` through [PR #8](https://github.com/gedxxe/Adaptive-LoRa/pull/8).
The [README](../../README.md) is the operator entry point.

| Item | Current evidence |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `3` |
| Board | Heltec WiFi LoRa 32 V3.4, ESP32-S3/SX1262, V3.2 electrical reference |
| ESP32 build | `heltec_wifi_lora_32_V3_multiclient` passed. Both legacy comparison images also passed. |
| Development roster | Gateway 1, Clients 2–9, each with a 64-byte budget. The advertised period is 6,766 ms. |
| Timing | Beacon and grant anchors use radio completion timestamps. Slot launches still use current time. |
| ADR research | The gateway records per-client first delivery, retry or miss. The offline evaluator uses experiment-supplied thresholds. |
| Host | 356/356 native regressions, 15/15 v3 runtime cases, and 23/23 in the final focused v3 link run |
| Physical acceptance | IN PROGRESS; NOT RUN |

M3 scheduling and M4 feedback/retries are implemented for the eight-Client
configuration and verified on the host. M5 has uplink capture and offline
analysis. On-air negotiation, retuning and recovery remain open. Start the
physical tests with one pair and expand through eight Clients.

The PHY, NVS schema, report interval, M2 ACK timeout, v3 wire layout and
relative radio slots are unchanged. See the [display guide](DISPLAY_DIAGNOSTICS.md)
for freshness and timing counters.
Client boot reserves up to 400 ms for a WAIT screen before beacon RX.
The operator reported about five metres of additional reach in a separate
SF9/CR 4/6 experiment, with intermittent stale status. No raw capture was
supplied for timing or range analysis. The v3 development target uses SF7.
See [build memory](BUILD_MEMORY.md) for the full regression record.
<!-- current-state:end -->

## Historical snapshot — Iteration 020

`codex/iteration-020-multiclient-payload-scheduling` stacks on the published
Iteration 019 commit `979d873fad629aedcdf0e8a565b4ad3681bbea61`. The main
baseline remains `541e2e8b985f7cd67ca4d1f1af323c46aed4333e`.

| Item | Current evidence |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `1` by default, with separate v2 and v3 research profiles |
| Board | Heltec WiFi LoRa 32 V3.4, ESP32-S3/SX1262, V3.2 electrical reference |
| Timing | Default report 3,000 ms, M2 ACK 1,000 ms. v3 targets 1,000 ms and admits up to 10,000 ms. The shipped roster derives 6,516 ms using a provisional 50 ms guard. |
| RF and storage | 915.0 MHz, SF7/BW125/CR4/5, 22 dBm requested, 140 mA PA OCP. Network/gateway/client defaults 1/1/2. Existing NVS schema 1 values persist. |
| ESP32 builds | `heltec_wifi_lora_32_V3`, `_oled`, `_research`, `_multiclient` all passed |
| Native | `native` 353/353 and `native_multiclient` 3/3 passed |
| Host tools | Capture 13/13, legacy model 16/16, v3 model 3/3, documentation checks 9/9 and direct checker passed |
| Physical acceptance | IN PROGRESS; NOT RUN |

The v3 test network exercises 1, 2, 4 and 8 client state machines. DATA grants
use the submitted length, and aggregate feedback grants shared retries only
to missing clients. A ten-client extrapolation fits the ten-second budget
under the model assumptions. The firmware admits up to eight. No physical
test or measurement was taken. See the [startup guide](STARTUP_AND_PROVISIONING.md),
[wire specification](SCHEDULED_LINK.md) and [build memory](BUILD_MEMORY.md).

## Historical snapshot — Iteration 019

Branch `codex/iteration-019-research-implementation` starts at published
Iteration 018, `0735bb8ef5cb7c1d677c98f2123c4a3f6cb7a1ea`. Canonical `main`
remains `541e2e8b985f7cd67ca4d1f1af323c46aed4333e`.

| Item | Current evidence |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `1` (default M2); opt-in scheduled v2 |
| Build environment | `heltec_wifi_lora_32_V3`, `_oled`, `_research`; all exit 0 |
| Board | Two owner-reported Heltec WiFi LoRa 32 V3.4, ESP32-S3/SX1262, stock monopoles, 902–928 MHz procurement; V3.2 electrical reference |
| Restored baseline | 915.0 MHz, SF7/BW125/CR4/5, 22 dBm request, 140 mA PA OCP, network/gateway/client 1/1/2, 10,000 ms reporting default and M2 ACK timeout, NVS schema 1 |
| Scheduled software | One client, payload-aware slots, cumulative bitmap feedback and three retries. Host tests run the actual state machines. |
| Software checks | Native 333/333, capture 13/13, airtime model 16/16 and documentation tests 7/7 PASS. Direct consistency check PASS. |
| Physical acceptance | IN PROGRESS; NOT RUN |

Follow the [M2 bench runbook](BENCH_RUNBOOK.md) first. M3/M4 acceptance
needs two boards running v2 and measurements of conducted power, TX/RX
turnaround, IRQ jitter and drift. See the [v2 specification](SCHEDULED_LINK.md),
[repository audit](../research/ITERATION_019_AUDIT.md) and [build memory](BUILD_MEMORY.md).
M5–M8 remain planned. No hardware was accessed this iteration.

## Historical snapshot — Iteration 018

Main includes PR #6 at `541e2e8b985f7cd67ca4d1f1af323c46aed4333e`.
The research branch is `codex/iteration-018-research-traceability`.

| Item | Current evidence |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `1` |
| Build environment | `heltec_wifi_lora_32_V3` and opt-in `heltec_wifi_lora_32_V3_oled` |
| Board | Heltec WiFi LoRa 32 V3.4, ESP32-S3/SX1262. V3.2 electrical reference. |
| Software scope | M2 HELLO/ACK. Reserved-type rejection test expanded. No firmware change. |
| Research | Four fixed papers. Fifteen figure entries, three attributed crops and six original SVG illustrations. |
| Software verification | Both S3 builds PASS and binary-identical to 016.1. Native 316/316, capture 13/13, airtime 16/16, docs 7/7 and direct checker PASS. |
| Physical acceptance | IN PROGRESS; NOT RUN |

[Research references](../references/README.md) and the
[traceability matrix](../research/ALGORITHM_TRACEABILITY.md) separate paper
claims, project design, host evidence and physical measurements. No M3–M8
production feature is active. Next physical step follows the existing
[BENCH_RUNBOOK.md](BENCH_RUNBOOK.md).

## Historical snapshot — Iteration 016.1

Client date 2026-11-19. PR #5 merged Iteration 015 into main at
`e34e7296135b47ecd3af479fe0593f4118bc734b`. The review branch is
`iteration-016-v34-physical-validation`.

| Item | Evidence |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `1` |
| Build environment | `heltec_wifi_lora_32_V3`, opt-in `heltec_wifi_lora_32_V3_oled` |
| Board | Two Heltec WiFi LoRa 32 V3.4 boards, ESP32-S3/SX1262, stock monopoles, owner-confirmed 902–928 MHz procurement. V3.2 is the electrical reference. |
| Native tests | 316/316 PASS, including nine new OLED/telemetry cases |
| Capture / model tests | 13/13 and 16/16 PASS, host only |
| Firmware builds | Both PASS. OLED off RAM 32,184 / flash 342,852 bytes. OLED on 33,960 / 377,488 bytes. |
| Documentation | 7/7 PASS and direct checker PASS |
| Physical acceptance | IN PROGRESS; NOT RUN |

The owner confirms V3.2 wiring and reports SMT/crystal improvements in V3.4.
Physical SKU, antenna markings and characteristics, USB, flash security and
layout, peripheral behavior and timing remain unverified. The local serial list shows
six Bluetooth virtual ports and no CP2102. Both rebuilt images match Iteration 016
byte-for-byte. The fixed 915.0 MHz
profile and opt-in OLED are preserved. M3–M8 production work remains unstarted.
The [bench runbook](BENCH_RUNBOOK.md) starts with one-board CP2102 enumeration,
then same-board backups, OLED checks and separate 15,000 ms provisioning.
See [HARDWARE.md](HARDWARE.md) for the evidence split and [BUILD_MEMORY.md](BUILD_MEMORY.md)
for software checks. No physical acceptance result has been recorded.

## Iteration 015 historical snapshot

### Iteration 015 review branch before merge

Client date: 2026-11-18. Canonical main is the merged Iteration 014 baseline
`0d0e71c368e88093b081bf9182e1f1babd189a9a` ([PR #4](https://github.com/gedxxe/Adaptive-LoRa/pull/4)).
Work is on `iteration-015-prehardware-stabilization`; its PR is for review,
with no automatic merge. Main remains canonical until review and integration.

| Item | Current evidence |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `1` |
| Target | Heltec WiFi LoRa 32 V3.2 HF, ESP32-S3FN8/SX1262 |
| Build environment | `heltec_wifi_lora_32_V3` |
| Hardware configuration | 8 MB flash, no assumed PSRAM; CP2102/UART0 115200; TCXO 1.8 V, DC-DC |
| Firmware build | PASS, exit 0 |
| Native tests | 307/307 PASS: 302 regressions plus five deduplication characterizations; no skipped cases |
| Capture tests | 13/13 PASS, fake serial only |
| Airtime/model tests | 16/16 PASS, host-only analytical model |
| Documentation consistency | 7/7 tests PASS; checker exit 0 |
| Physical acceptance | IN PROGRESS; NOT RUN |
| M3–M8 production | NOT STARTED; preliminary host-only research is separate |
| Resource summary | 32,096 bytes RAM, 342,560 bytes flash; unchanged from Iteration 014 |

The one-image NVS-role architecture, schema 1, fixed SF7/BW125 PHY and v1
HELLO/ACK behavior are unchanged. All 38 production/config/version input
hashes and the firmware binary match the baseline; main.cpp remains 15 lines.
Fresh logs/results use `logs/build/iteration-015-verification-esp32*` and
`logs/test/iteration-015-verification-{native,capture,model}*`.
The documentation checker and its results are recorded in BUILD_MEMORY.

AGENTS is now a shorter current contract. The active architecture diagram and
boot order are corrected; CODEX_START_PROMPT is explicitly archived. Previous
progress snapshots below retain their own hardware, outcomes and publication
checkpoints. New design/tooling is described in [TDMA_FEASIBILITY.md](TDMA_FEASIBILITY.md).
The 216 analytical scenarios use a hypothetical ten-second superframe and
unknown switching/processing/guard costs; they do not establish node capacity.
Eight-session cache tests demonstrate eviction/reboot limits without changing
production deduplication policy.

Resolved environment: PlatformIO Core 6.1.19, installed espressif32 55.03.311,
Arduino 3.3.11/IDF libs 5.5.5+sha.b774170ff46, Xtensa GCC 14.2.0+20260121,
RadioLib 7.8.1, native 1.2.1, Unity 2.6.1 and host GCC 6.3.0. The platform is
not pinned; reproduce against the recorded manifests rather than assuming a
new installation selects the same packages.

ERR-008/009/010 synchronous I/O/allocation limitations, ERR-021 TX BUSY wait
and ERR-023 pre-runtime NVS formatting remain open. No new production defect
was reproduced. No real port, upload, erase, reset, NVS or RF test was used.
Board receipt/markings, supply, IRQ/TCXO timing, heap/stack peaks, entropy,
antenna matching/gain and radio reliability remain unmeasured.

Next: inspect both delivered V3.2 HF boards, terminate RF before power, verify
same-board private ROM backups/security/layout, then execute the
[BENCH_RUNBOOK.md](BENCH_RUNBOOK.md). Capture the controlled normal/negative/
recovery cases and measure switching, processing and drift before an M3
implementation review. ERR-021 recovery acceptance remains a separate hold.


## Historical snapshots through Iteration 014

The snapshots below are historical records. Their hardware inventory, build
commands and words such as "current" apply only to their original iteration.
Detailed chronology and corrections remain in BUILD_MEMORY and ERROR_LOG.

<details>
<summary>Earlier progress snapshots and historical acceptance state</summary>

# Development progress

## Iteration014: Heltec V3.2 HF software migration complete

2026-11-17 client date. The sole active target is Heltec WiFi LoRa 32 V3.2 HF,
ESP32-S3FN8/SX1262, with CP2102/UART0,1.8 V TCXO and DC-DC selection.
The user explicitly authorized immediate implementation and preservation
of engineering history. Baseline main075b8faf includes PR #3; the dedicated
migration branch is iteration-014-heltec-sx1262-migration-audit. Publication
is through a normal-merge PR after review; the final PR state identifies
its merge SHA.

| Software evidence | Result |
|---|---|
| S3 firmware build | PASS, exit0 |
| Native tests | 302/302 PASS: all290 original cases plus12 adapter cases |
| Capture utility | 13/13 PASS, fake ports only |
| Protocol / NVS / firmware | 1 / schema1 / 0.2.0-alpha.1, unchanged |
| Main / app/core policy | 15-line main;25 protected original files byte-identical |
| RAM / flash | 32,096 /342,560 bytes; no physical heap/stack measurement |

The initial native fixture duplicate declaration failed compilation and is
preserved in the migrated-native log; corrected retry passes all cases
(ERR-025). Source settings and vendor documents are recorded in
HARDWARE_MIGRATION_AUDIT.md and iteration-014 artifacts. Current PHY remains
915 MHz/BW125/SF7/10 dBm. No display/battery, scheduler, bitmap, selective
retry or ADR feature is added.

Board receipt/markings, supply, UART/reset, NVS/flash, SPI/IRQ/TCXO/RF,
antenna matching and runtime memory/entropy remain unverified. ERR-021
and ERR-023 are open; previous CDC-specific evidence is historical after
UART0 selection. M0-M2 physical acceptance remains IN PROGRESS; M3-M8
have not started. Next: procure/inspect the fixed target, terminate RF,
make same-board private backups, then follow the current bench runbook.
Do not use the earlier removable-radio single-MCU procedure on this board.

## Historical snapshots through Iteration013

The records below describe their own hardware and publication checkpoints.
Their inventory/build commands are not the active target configuration.

## Iteration013 main integration - validation complete

2026-11-13. PR [#3](https://github.com/gedxxe/Adaptive-LoRa/pull/3) integrates verified013 `e431e3f8ffd6e2d95dc126bb68e8231723a13c4d` into initial main `5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861` using a normal merge commit after successful validation. All113 divergence paths reviewed; main2 unique commits have no tree changes against the common ancestor, so the conflict-free candidate tree equals013. No production/test/tool inputs change; integration documentation/logs only. This snapshot records the tested candidate; the linked PR supplies final publication state and merge SHA, independently checked against remote main. After merge, **main is the canonical development baseline** (ADR-017), and all historical iteration branches/records remain.

Fresh ESP32 exit0 PASS, native exit0 **290/290 PASS**, capture exit0 **13/13 PASS** with fake serial only. All55 input fingerprints and firmware.bin match published013; RAM24,564/flash364,308, firmware0.2.0-alpha.1/protocol1/NVSschema1/RadioLib7.8.1/pins/fixedPHY unchanged; main.cpp remains15 lines. Full evidence: `logs/build/iteration-013-main-integration-validation.json`, `iteration-013-main-integration-review.json`, corresponding build/native/capture logs and per-case scenarios. Source e431e3f remains in ancestry; no squash/rebase/force/branch deletion.

Physical NVS/reset/USB/SPI/IRQ/RF/antenna/entropy/heap/stack NOT VERIFIED/NOT MEASURED. ERR-005/008/009/010/021/023 remain; M0–M2 physical IN PROGRESS and M3–M8 NOT STARTED. Next development starts from synchronized main; existing backed-up MCU/pair gates and dependency recovery holds still apply.

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

## Historical current baseline (pre-Heltec snapshot)

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

## Historical current status (pre-Heltec snapshot)

Iteration-007 baseline evidence is preserved: candidate `0.2.0-alpha.1` / protocol 1, compile PASS and 92/92 host cases. Iteration 008 re-executed those checks after identity changes: 38 production-app scenarios, codec 18, role/config 13, console 10, sequence/timing 7, logger 6. These are scenario counts, not line/branch coverage percentages.

Corrected stock: two LLCC68 modules, one MCU, second MCU not purchased, Moxon not fabricated; no assembled pair. Physical NVS/radio/RF tests: NOT RUN. The actual app statement bodies are unchanged; only the transport constructor type and standard C++ format-header includes differ. Main remains unchanged at 15 lines. Audit/proposal/results: SOFTWARE_AUDIT.md; exact cases: `logs/test/iteration-007-scenarios.md`. No complete milestone is PASS.

| ID  | Milestone                                                      | Status      | Evidence                                 |
|-----|----------------------------------------------------------------|-------------|------------------------------------------|
| M0  | Board, module, pin, power, antenna, and toolchain confirmation | IN PROGRESS | Resolved toolchain recorded; physical checks pending |
| M1  | LLCC68 initialization on both devices                          | IN PROGRESS | Firmware compiles; physical initialization/NVS persistence pending |
| M2  | Fixed-PHY HELLO/ACK                                            | IN PROGRESS | 290 native tests including apps/driver/store/runtime/reliability pass; RF evidence pending |
| M3  | Beacon and one-client TDMA                                     | NOT STARTED | Deferred until M2 passes                 |
| M4  | ACK bitmap and selective retry                                 | NOT STARTED | Deferred until M3 passes                 |
| M5  | Versioned ADR SF7/SF8/SF9                                      | NOT STARTED | Deferred until M4 passes                 |
| M6  | Synchronization loss and rejoin                                | NOT STARTED | Deferred                                 |
| M7  | Controlled Moxon RF evaluation                                 | NOT STARTED | No controlled RF data recorded           |
| M8  | 5–70 logical-client host simulation                            | NOT STARTED | No simulation recorded                   |

## Historical toolchain record (pre-Heltec snapshot)

| Item                            | Resolved value                                       |
|---------------------------------|------------------------------------------------------|
| PlatformIO Core                 | 6.1.19                                               |
| PlatformIO espressif32 platform | 55.03.311 / display 55.3.311, installed pioarduino fork |
| Arduino-ESP32 core              | 3.3.11; framework libs 5.5.5+sha.b774170ff46          |
| Compiler                        | RISC-V GCC 14.2.0 (esp-14.2.0_20260121)              |
| RadioLib                        | 7.8.1 pinned and resolved                           |
| Host compiler                   | MinGW GCC 6.3.0                                     |
| Host test framework             | Unity 2.6.1 / PlatformIO native 1.2.1                |

## Historical open hardware checks (pre-Heltec snapshot)

- [ ] Confirm exact ESP32-C3 Super Mini board revision and flash size.
- [ ] Confirm exact Ra-01SCH module marking and pin orientation.
- [ ] Verify all proposed GPIO connections by schematic/continuity.
- [ ] Confirm radio 3.3 V rail and measure it during TX.
- [ ] Confirm antenna connector, feed line, and 50-ohm path.
- [ ] Confirm local frequency/channel/power requirements before
  deployment.
- [x] Record the exact toolchain versions after first successful build.

## Historical current blockers (pre-Heltec snapshot)

ERR-011 (legacy MinGW format macros) is fixed by C++ `<cinttypes>` and verified by 92 passing native cases/ESP32 compile. No application packet/state defect was reproduced in those scenarios. ERR-008 synchronous NVS/BUSY, ERR-009 dependency scratch allocations, ERR-010 SDK serial waits and ERR-005 physical CDC behavior remain open limitations; hardware IRQ/random-source assumptions are also unverified. Native driver fault tests cover policy around returned operations, not the actual dependency timing/allocation or ISR interleavings.

ERR-007's header filter remains unchanged from iteration 005; physical IRQ behavior is still an M1/M2 gate. Migration changes are mechanical namespace/macro labels and a byte-equivalent storage namespace representation; behavior equivalence is checked across 35 source/header/test files. New source/binary fingerprints are in iteration-008 records; older hashes identify original artifacts. Git was initialized only after the pre-edit identity audit. No new scheduler, bitmap, selective retry or ADR code exists.

## Historical next action (pre-Heltec snapshot)

Next physical action: use the Iteration013 backed-up MCU-only procedure once a board is identified; complete procurement/instruments and actual M0 checks. ERR-021 and controlled replay/NVS fixture gates must be resolved before full M1/M2 closure; native/software evidence is already verified. Physical gates wait for the second MCU, antenna and assembly; then follow BENCH_RUNBOOK.md for M0/M1 and >=200 actual M2 HELLO attempts. Do not start M3 until M2 acceptance evidence exists. TDMA/bitmap/retry scheduling/ADR remain unimplemented.

Final evidence-reader check: Windows default decoding failed on a UTF-8 build log; explicit UTF-8 succeeded (ERR-006 recurrence). Firmware/native PASS evidence is unchanged; no new firmware error.

Iteration 009 preservation: RadioDriver state/deadline/error policy remains equivalent after hardware forwarding/constant substitutions; 34 protected original files unchanged. Runtime owns one Llcc68Hardware plus a reference-injected RadioDriver; main.cpp remains 15 lines. The new seam has no explicit heap allocation. RAM 24,532 / 327,680 bytes; flash 363,730 / 1,310,720. Actual adapter/SPI/BUSY/ISR races, Runtime/entropy/NVS/USB and RF remain outside native evidence; M0–M2 overall IN PROGRESS, M3–M8 NOT STARTED.

Iteration 009 review note: staged log display whitespace triggered ERR-015 (check exit 2); log-format normalization is verified closed by staged check exit 0; source/build/test evidence unaffected.

Iteration 010 preservation: NvsConfigStore error/count/cleanup/readback logic and numeric namespace are equivalent after reference forwarding; key `device`, schema 1 and 24-byte format unchanged. Runtime header constructs one PreferencesConfigStorage before its store; runtime implementation and 39 protected original files (including all 163 old test cases) are byte-identical. All 48 frozen inputs match. Main remains a 15-line entry point; no explicit allocation introduced. RAM 24,532 bytes unchanged, flash 363,854 (+124). No new warning messages; ERR-005/008/009/010 remain. Actual NVS/flash durability, power interruption, reboot/corruption recovery, wear/retention and physical radio/RF remain unverified; M0–M2 overall IN PROGRESS, M3–M8 NOT STARTED.

</details>
