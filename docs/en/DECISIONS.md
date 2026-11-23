# Architecture Decision Record

Append decisions. Do not silently change a decision in another document;
record the reason and update all affected documents.

## ADR-001 — One firmware image with NVS role selection

- Status: ACCEPTED
- Date: 2026-11-12
- Context: Gateway and client use identical ESP32-C3 Super Mini and
  Ra-01SCH hardware.
- Decision: Maintain one codebase and one firmware image. Load `GATEWAY`
  or `CLIENT` from validated NVS configuration at boot. If missing or
  invalid, enter provisioning mode with normal radio TX disabled.
- Consequence: Both applications are compiled into one image, but only
  one is activated at runtime. No hardware role detection is attempted.

## ADR-002 — LLCC68 driver, not SX1262

- Status: ACCEPTED
- Date: 2026-11-12
- Context: The installed Ai-Thinker Ra-01SCH module is based on LLCC68.
- Decision: Use RadioLib `LLCC68` and pin RadioLib 7.8.1 for the first
  implementation.
- Consequence: Do not copy SX1262-specific initialization or unsupported
  PHY combinations. Verify LLCC68 constraints.

## ADR-003 — Fixed-PHY HELLO/ACK before MAC scheduling

- Status: ACCEPTED
- Date: 2026-11-12
- Context: Only one gateway and one client are physically available.
- Decision: M0–M2 establish hardware, radio bring-up, and a fixed-PHY
  HELLO/ACK link before implementing TDMA/ADR.
- Consequence: The first RF result is a two-device link test only.

## ADR-004 — Gateway-scheduled TDMA

- Status: ACCEPTED AS DESIGN DIRECTION
- Date: 2026-11-12
- Context: A single LLCC68 cannot receive multiple spreading factors
  simultaneously and must serve clients sequentially.
- Decision: Use a gateway beacon and scheduled client slots. Gateway
  switches its receiver to the expected client’s PHY before each slot.
- Consequence: Slot timing must include airtime, radio switching,
  processing, and measured clock guard. Do not claim multi-node
  collision avoidance before M3/M4 validation.

## ADR-005 — ACK bitmap and deterministic selective retry

- Status: ACCEPTED AS DESIGN DIRECTION
- Date: 2026-11-12
- Context: The network needs explicit delivery status without a separate
  ACK packet per client.
- Decision: Use a schedule-versioned bitmap ACK and deterministic retry
  slots. Preserve packet identity across retries and deduplicate at the
  gateway.
- Consequence: 70 clients require 9 bitmap bytes. Host tests must cover
  1, 10, and 70 clients before field use.

## ADR-006 — Conservative gateway-controlled ADR

- Status: ACCEPTED AS DESIGN DIRECTION
- Date: 2026-11-12
- Context: Per-node RF conditions may vary across a factory.
- Decision: Gateway controls PHY changes using rolling link evidence,
  hysteresis, and versioned propose/accept/commit. Initial adaptation is
  SF7–SF9 at BW125 only.
- Consequence: Do not change PHY mid-superframe. Do not change bandwidth
  or TX power in the first ADR release. Thresholds remain experimental
  until measured.

## ADR-007 — Keep cloud and meter integration out of M0–M2

- Status: ACCEPTED
- Date: 2026-11-12
- Context: Radio link and protocol correctness need independent
  validation.
- Decision: Do not add Wi-Fi, Modbus, PostgreSQL, Vercel, or dashboard
  code in the first firmware delivery.
- Consequence: Add gateway backhaul and meter acquisition as a separate
  integration phase after the radio MAC is stable.

## ADR-008 — Non-monolithic application structure

- Status: ACCEPTED
- Date: 2026-11-12
- Context: Large role-specific `main.cpp` files are difficult to review
  and maintain.
- Decision: Keep `main.cpp` thin; separate runtime, provisioning, radio,
  protocol, gateway app, and client app responsibilities.
- Consequence: Reject changes that move packet parsing, RadioLib calls,
  retries, or ADR decisions into `main.cpp`.

## ADR-009 — Bounded M2 persistence, console, replay window and diagnostics

- Status: ACCEPTED for implementation candidate; date 2026-11-12.
- Decision: NVS schema 1 stores one explicit 24-byte little-endian blob (magic, schema, role, IDs, HELLO interval, reserved bytes, FNV-1a integrity value). The hash detects storage corruption; it is not a wire checksum or authentication. Load and save validate the complete config; save reads back. Failed load keeps UNSET; only reboot activates staged configuration.
- Console uses a 128-byte line buffer, six-token bound and 64-byte loop budget. Add `config set hello-interval-ms <1000..3600000>`; default 5000 ms between completion and next attempt. A role edit preserves IDs; provision all IDs explicitly as in README. Network 65535 is allowed; broadcast reservation applies to node addresses.
- M2 HELLO permits 0–68 opaque bytes (client emits empty payload). ACK is exactly nine bytes, accepted status 0 only. M2 rejects broadcast, future reserved message types and nonzero frame/config/schedule versions; v1 header unchanged.
- Deduplication retains eight session keys with a 64-sequence window each. Older sequences in a retained session are rejected as stale. Oldest session eviction and gateway reboot lose replay history; M2 is neither authenticated nor persistent replay protection. No DATA implementation is added.
- Fixed data PHY SF7 only, private sync word 0x12, XTAL (DIO3 voltage 0), LDO mode; exact module oscillator/regulator/RF-switch behavior remains a bench check. RadioLib default internal DIO2 RF-switch handling is retained; TXEN/RXEN are not MCU-controlled.
- Native USB CDC enabled for the provisional C3 target; verify actual board USB path before flashing. Pins remain provisional.
- Logging uses a bounded 16-line queue and nonblocking drain. Overflow is counted in `status`; loss of required evidence requires repeating the run. RadioLib does not expose the matched chip ID string or status for every metric getter; report driver selection honestly, not silicon identification.

## ADR-010 — Test production M2 apps through a minimal transport reference

- Status: ACCEPTED; date 2026-11-12; scope software-only M0–M2 verification.
- Context: concrete RadioDriver headers pulled Arduino/RadioLib into both apps/logger and prevented current native tests from exercising production transitions. Read-only audit and concrete seam proposal preceded edits (SOFTWARE_AUDIT.md).
- Decision: introduce only six already-used operations in RadioTransport, with bounded hardware-independent RadioEvent. RadioDriver implements this one interface and remains the sole hardware/LLCC68/SPI owner; Runtime retains concrete begin/poll/stop. Apps use a reference, no heap ownership or framework.
- Tests compile the actual app/codec/tracker/config/logger .cpp files. Native-only compatibility headers provide virtual ESP timer and captured Serial byte writes; fake transport injects completed packets/events and errors, not RF/IRQ/SPI behavior. Runtime/NVS are audited, not executed by these app tests.
- No M2 retry policy is added. Test retransmission means replaying byte-identical captured packets; a new application transaction still uses a new sequence. Volatile deduplication boundaries and gateway-reboot history loss remain documented behavior.
- Correction to ADR-009 wording: a bounded logger queue/capacity check does not establish nonblocking behavior of SDK USB calls. Synchronous NVS/BUSY/USB and RadioLib scratch allocation remain explicitly unverified dependencies, not simulated successes.
- Inventory update: two modules/one MCU/no fabricated antenna, no assembled pair. Physical gates remain open; later MAC milestones are not started.

## ADR-011 — Standalone Adaptive-LoRa identity and publication boundaries

- Status: ACCEPTED; date 2026-11-12; firmware 0.2.0-alpha.1 and protocol 1 unchanged.
- Project identity: Adaptive-LoRa. Research title: Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks. Industrial energy monitoring is a general motivation, not a protocol dependency or deployment affiliation.
- Explicitly authorized identity/path redaction applies to historical publication copies while preserving chronology, timestamps, versions, results, errors and resolutions. Private originals remain outside publication; historical hashes are fingerprints of original artifacts. Publish a digest/redaction manifest and mark provenance rather than fabricate revalidation.
- Rename internal C++ namespace/native-test identifiers mechanically. Preserve the legacy NVS namespace bytes as a neutral numeric compatibility constant, and preserve storage schema/key/blob, wire fields, pins, PHY and state logic. No storage migration or protocol/version change.
- Retain current RadioTransport/LLCC68 ownership and document platform/application gaps. No generic application payload/event or complete platform-service API is claimed, and no SX1262 implementation exists. Meter/backhaul/database/UI dependencies must stay outside core protocol logic.
- Initialize main only at the project root, retain any fetched remote commits, review sanitized staged files and publish only to the explicitly authorized repository. The remote is currently empty, so no unrelated-history merge is required. No force push, visibility change or automatic tag.
- SDKs/binaries/local configuration/private originals are excluded; technical development records remain tracked. Project license selection is deferred, dependency license references retained, and unlicensed full-text research papers are not included.

## ADR-012 — Verify production RadioDriver through one hardware reference

- Status ACCEPTED; date 2026-11-12; iteration 009, firmware 0.2.0-alpha.1 / protocol 1 unchanged. Pre-edit audit/proposal: RADIO_DRIVER_AUDIT.md.
- Split only the existing radio module: RadioDriver retains all state, bounds, deadline and event/error policy; Llcc68Hardware owns Module/LLCC68/SPI/GPIO/ISR. A non-owning RadioHardware reference exposes only existing hardware calls; Runtime owns exactly one adapter before one driver. No heap/factory/tasks or second adapter.
- Application-facing RadioTransport remains unchanged. Native tests compile actual RadioDriver.cpp and inject return codes, IRQ latch/level/raw flags, payload bytes and explicit uint32 timestamps through a bounded fake, without duplicating driver state logic.
- Numeric status/raw-IRQ values are checked against pinned RadioLib with production compile-time assertions. Fixed initialization/receive arguments and one-flag ISR are moved unchanged. No general PHY/lifecycle configuration service or physical-driver emulator is claimed.
- Preserve existing priorities and lifecycle semantics, including HEADER_VALID qualification, timeout precedence, retained initialized flag after a repeated begin failure, and metrics from a successful read even if final cleanup fails. This task changes testability, not recovery policy.
- Host tests do not run the real adapter, SPI/BUSY/ISR interleavings or RadioLib internals; synchronous waits/allocations, physical NVS/RF and other hardware gates remain open.

## ADR-013 — Verify production configuration policy through five storage operations

- Status ACCEPTED; date 2026-11-12, iteration 010. Pre-edit findings/proposal: CONFIG_STORE_AUDIT.md. Baseline is the verified Iteration 009 remote HEAD, not the subsequently merged main.
- ConfigStorage exposes begin, getBytesLength, getBytes, putBytes and end, held by non-owning reference in NvsConfigStore. One PreferencesConfigStorage owns one Preferences instance and forwards existing namespace/key arguments/results without policy. Runtime constructs the adapter before the store; usage stays serialized and non-reentrant, with each successful begin explicitly ended before readback reopens.
- Keep validation, encoding/decoding, error strings/order and readback comparison inside actual NvsConfigStore; native compiles that production implementation and actual DeviceConfig. The bounded fake contains only byte storage, response plans and operation traces; no schema/codec/store model. No heap ownership/factory/filesystem/general storage framework.
- Original numeric namespace initializer remains identical in NvsConfigStore and its historical label is not decoded. Key device, schema 1, 24-byte LE/FNV format and pure config validator remain unchanged. Firmware 0.2.0-alpha.1, protocol 1, RadioLib 7.8.1, radio/apps/PHY/console/boot dispatcher are protected.
- Failed save does not promise rollback or prove commit durability; readback failure propagates the original load error, not a generic verify error. Valid but different readback remains nvs_verify_failed. Store output remains unchanged after failed load; loaded runtime config is still activated only by the existing reboot workflow.
- Native verifies store policy against simulated responses, not Preferences/NVS internals, physical flash commit/power loss, reboot/corruption recovery, wear or retention. Defensive encode-false branches are retained but not falsely claimed injected. Dependency timing/allocation and hardware gates remain open.

## ADR-014 - Execute the single production coordinator with native SDK adapters

- ACCEPTED 2026-11-12, iteration011; pre-edit RUNTIME_AUDIT.md records the audit/proposal. Firmware0.2.0-alpha.1/protocol1 unchanged.
- FirmwareRuntime receives existing ConfigStorage/RadioHardware references and still owns the actual store, driver, console, logger and optional role apps. No new lifecycle/platform interface, factory, tasks or duplicate coordinator. Esp32Firmware composition solely owns concrete Preferences/LLCC68 adapters before runtime; references outlive it. Main constructs that composition and delegates only (15 lines).
- Actual runtime.cpp policy and freshBootId stay unchanged; only use C++ cinttypes for the legacy native host. Existing native Arduino/esp_timer pattern is extended with bounded serial input/micros/reset/reason/raw-random shims. Same production apps/store/driver/codec/log statements run. Native compatibility headers are not hardware emulators or general production MCU ports.
- Firmware selects standard optional; installed GCC6.3 host selects experimental optional through a project namespace alias when standard header is absent. Both use existing inline emplace/bool/dereference operations, no namespace std changes or heap ownership. Native UNITY_SUPPORT_64 is required consistently in Unity and tests after reproduced ERR-017; no assertions truncated/removed.
- Existing lifecycle policies are characterized, not changed: app update before serial parsing, no hardware poll/stop calls before radio init, staged save until new boot, stop failure still permits reset, reset after100ms captured-clock deadline without log-empty gate, existing nonzero/previous-ID rejection loop. RNG failure liveness and inter-boot collisions are not solved by this seam.
- Low-level fake replies plus the actual production coordinator verify software ordering; simulated object recreation/reset/raw random/serial capacity do not prove physical reset/NVS/USB/entropy/SPI/IRQ/RF. No later MAC or application integration is added.

## ADR-015 — Bounded session renewal and fresh runtime boundary timestamps

ACCEPTED2026-11-12, Iteration012; pre-edit findings and finite RED evidence in DEPENDENCY_BUDGET_AUDIT.md, ERR-019/020. Production generator makes at most8 raw calls and returns0 on unavailable valid value; keep uint32(previous) API with documented failure sentinel, not a second platform interface. SequenceCounter.renew rejects0/previous and preserves old boot/exhausted count on failure. Both apps log SESSION_ERROR/boot_id_unavailable, inhibit normal TX and retry once per1s attempt-start deadline; success logs identity_recovered. Reboot-pending disables recovery. No predictable fallback, per-packet NVS, boot-counter/schema change or entropy-source enablement. The bound assumes raw SDK calls return; it is not a measured WCET or uniqueness/authentication guarantee.

Actual delayed-operation tests reproduced expired-ACK acceptance, early ACK-window anchoring, shortened reset wait and delayed reset checks. Refresh app time after poll, arm100ms after completed stop and evaluate current time after drain. Retain app-before-serial, failed-stop-still-reset, no-log-empty gate, driver/app single-call observation-time/recovery semantics and non-preemptible dependencies. All266 previous cases unchanged/passing;24 new pass; ESP32 passes. New state costs16 staticRAM bytes, flash+466; no new explicit allocation/task/framework. main unchanged15 lines.

Wire/NVS/versions/PHY and one image remain compatible. Installed IDF5.5.5 source constrains entropy claims;32-bit identifiers can collide across boots. Persistent session counter merits a separately versioned power-loss/wear design later. STATIC_ONLY remains0 because it changes stack/capacity without eliminating HAL/SDK allocation. Upstream BUSY-ramp and SDK locks/flash/USB timing remain open, and hard timing/physical acceptance is not established.

## ADR-016 - Backed-up physical gates and capture evidence contract

ACCEPTED2026-11-13, Iteration013. Firmware/native sources and all53 inputs remain byte-identical to012; no architecture/protocol/PHY/watchdog/library change. Physical acceptance is separate from procedure/software readiness; missing hardware, actual wiring/authorization and ERR-021/controlled fixtures remain explicit holds. A configured radio-less MCU still attempts LLCC68 initialization. Use Gateway staging for MCU-only persistence checks with radios physically absent, private same-board ROM backups and verified restore; no unbacked erase/config overwrite, encrypted/secure/incompatible layouts need a separate plan. SDK startup formatting risk ERR-023 is documented, not silently patched.

Capture schema2 retains existing pair/status-only use and adds a single-board mode, pre-open modem-line intent (not reset immunity), timestamp/offset journals, hashes/fragments, operator metadata bound, initial/final state and interrupted/error disposition.13 fake-port cases address demonstrated incomplete-success/evidence gaps (ERR-022), not extra firmware coverage. Default final status/tail is best effort; missing counters/ambiguous logs block evidence. Raw upstream loss is not eliminated by file hashes. Public records exclude private backups/device identifiers until reviewed.

CSV analysis uses a closed HELLO-identity cohort with distinct attempts/completions/receives/ACK starts/completions/accepted counts. Start failures remain in A; PDR uses R intersect completed T; zero denominators/unknown associations stay N/A. Host observation timestamps and firmware transaction_us are not physical RF timing. Normal >=200 completed HELLO run, interventions and injection fixtures remain separate. Earlier physical records not fabricated or overwritten.

## ADR-017 - Canonical main baseline after Iteration 013 integration

ACCEPTED 2026-11-13, explicit user synchronization request. Integrate verified source e431e3f8ffd6e2d95dc126bb68e8231723a13c4d and its Iteration 011/012 history into main through PR #3 with a normal merge commit only. Initial main5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861 contains two unique merge records but no tree difference from the common Iteration 010 ancestor; the conflict-free candidate tree equals the verified source. Preserve both parent histories, every historical branch and all prior records. Add only integration documentation and executed validation logs; no production/test/tool input changes.

Use main as the canonical baseline after successful merge, with future work branched from current main. Rerun ESP32 build,290 native tests and13 fake-serial capture checks before integration; compare final merge tree and inputs to the tested candidate. Firmware0.2.0-alpha.1, protocol1, NVS schema1/namespace/key/24-byte blob, RadioLib7.8.1, fixed PHY and provisional pins remain unchanged relative to013. Existing011/012 fixes are retained, not reimplemented. No physical validation, later MAC feature, squash/rebase/force push or branch deletion is authorized by this decision.

## ADR-018 - Single Heltec V3.2 HF production target

Accepted2026-11-17 (client date), Iteration014. After the original audit-only prompt, the user explicitly authorized implementation while preserving engineering history, and directly requested publication/normal merge. Use Heltec WiFi LoRa 32 V3.2 HF as the sole active hardware target. Replace the active adapter and environment rather than maintaining two production targets; keep past records and Git branches intact.

The official V3 board definition is supported by installed platform55.03.311; no platform upgrade is needed. Use S3FN8/SX1262, GPIO8-14 radio mapping, DIO3 TCXO1.8 V and DC-DC, with source evidence in HARDWARE_MIGRATION_AUDIT.md. Common RadioLib7.8.1 begin already enables DIO2 RF switching. Retain driver's explicit-header/CRC reassertions and all policy/error tests. Select CP2102/UART0 using CDC_ON_BOOT=0; inherited board USB_MODE does not enable CDC. Leave OLED/Vext/battery features absent. V3.2 GPIO36 is low-active and GPIO37 battery measurement high-enabled, reserved for separately reviewed peripheral work.

Keep firmware0.2.0-alpha.1, protocol1, NVS namespace/key/schema1/blob, all app/codec/session/driver state policy and main.cpp unchanged. The selected-driver label changes to SX1262. Maintain915 MHz/10 dBm and the original fixed research PHY; the Moxon is a design baseline, with no fabricated match/gain/RF result. No scheduler/ADR or application integration. ERR-021 remains inherited and open; no third-party patch/watchdog modification. Source migration/build completion does not complete physical M0-M2.

Native compiles the actual new adapter through bounded test-only SPI/RadioLib call recorders, preserving all290 original cases and adding12. The sole old assertion change is the driver label. Initial duplicate fixture declaration failure is preserved (ERR-025); corrected full302 and capture13 pass. These tests verify arguments/forwarding and actual driver interactions, not chip behavior or physical IRQ/TCXO/regulator/RF. Platform/app integration gaps remain. Current operator docs require RF termination even for a single integrated-radio board and same-board private ROM backups; cross-MCU image restore is not a migration procedure.


## ADR-019 — Current contract and host-only scheduling research

Accepted 2026-11-18, Iteration 015. Use a concise AGENTS contract with stable
links to PROGRESS, ownership, acceptance and historical records. Mark the old
CODEX_START_PROMPT as archived and separate current progress from historical
snapshots. A small checker derives board/environment and version facts from
production/config files, checks selected current links/status, and excludes
archives; no permanent test-count or commit-SHA invariant is introduced.

Add Python-standard-library airtime/reservation tooling and a single bilingual
TDMA_FEASIBILITY design note. Unknown timing inputs remain explicit, ACK costs
include complete frames/metadata, retries reserve full rounds and all outputs
are analytical hypotheses. Do not activate reserved messages, scheduler,
bitmap/retry/ADR, change versions or alter production policy. Characterize the
eight-session cache rather than enlarging it; admission/schedule-associated
state is a future M4 decision. ERR-021 stays open without a proven dependency
mitigation. Preserve all historical records. Publish the named branch/PR for
review; this iteration does not authorize automatic merge into canonical main.


## ADR-020 — Optional onboard OLED diagnostics

Accepted 2026-11-19. Keep the original V3 firmware environment as an OLED-off
reference. The OLED candidate extends it with the same board, firmware version,
RadioLib pin, PHY and NVS layout. Use the installed Arduino Wire library and
project-local SSD1306 commands/glyphs. This avoids importing the Heltec
framework and a full pixel framebuffer. The vendor controller/address and V3
variant aliases support that implementation choice. Delivered V3.4 wiring,
RF variant and component revisions require physical evidence.

Esp32Firmware owns the display. Runtime starts Vext/reset sequencing and
later sends read-only snapshots after radio, app and serial work. One changed
page chunk is serviced per eligible loop. The display stops on I²C failure
and logs the reason once. The OLED skips TX, client ACK wait and reboot.
Wire allocates buffers and uses SDK locks. The configured timeout and chunk
limit bound requested I/O, while actual service latency needs measurement.

Telemetry uses application events, separating ACK starts and completions and
keeping a validity flag for receive metrics. These diagnostics wrap counters
modulo 2^32 and reset at reboot. Link state uses a monotonic freshness window
from the last qualifying receive. Preserve byte-level HELLO/ACK and schema 1.
Keep the bench frequency at its existing research value until an authorized
operating profile is selected. Physical evidence is required before M2 closes.

## ADR-021 — Separate v2 research image and restore the fixed PHY

2026-11-20. Keep the default and OLED profiles on protocol 1 and use an
explicit PlatformIO research profile for protocol 2. Both profiles use the
existing NVS role and schema 1. Each wire decoder accepts its own version.
Both roles use fixed SF7, one radio and its existing driver. The v2 calendar
has one peer, uses serviced beacon completion anchors and derives slots from
the advertised payload budget. An explicit missing bit and an unknown
result both allow a retry in the next slot, with the same DATA bytes. An
accepted bit must match the client boot ID and sequence. The retry budget is
one initial transmission plus three retries, as confirmed by the user.

Restore the confirmed 10,000 ms reporting default and 10,000 ms M2 ACK
timeout. Saved intervals still load as recorded. The scheduled profile uses
shorter feedback windows within that budget. Restore the 22 dBm SX1262
request and set its 140 mA high-power OCP after RadioLib `begin()` preserves
60 mA. The PA limit does not specify board input current. Keep V3.2 as the
electrical reference for the V3.4 pair. Bench evidence is needed for actual
output and the experimental guard. ADR remains planned.

## ADR-022 — Exact-length multi-client grants and shared retries

2026-11-20. Keep the v1 M2 and v2 single-client wire formats. Add v3 in its
own build environment. Only gateway-originated BEACON, SLOT_GRANT and
ACK_BITMAP may use the group destination. Request and DATA use the client
source ID and unicast gateway destination. A fixed roster limits admission
to eight clients. The gateway grants exact lengths after receiving requests
and uses the roster index for data ordering and both bitmap masks.

Use a bounded, copied TX buffer and synchronous RX callback. A positive bit
requires the callback to accept DATA and identifies the accepted client
session. Declined delivery remains unset. The gateway retains one packet
per active position for the frame. Each retry round reserves up to four
shared slots and grants them only to missing candidates. Without a current
grant, the client stays silent. Cumulative feedback can recover a lost ACK.

Use a 3,000 ms reporting default and a 1,000 ms M2 ACK timeout. Existing
NVS values remain authoritative. Preserve the 1,000 ms research target and
derive a feasible superframe from the roster budgets, up to 10,000 ms. Keep
the 50 ms provisional guard independent of those settings. v2 retains its
own 10 s feedback budget. Both scheduled images use fixed SF7. Coordinated
PHY changes need a versioned activation and fallback agreement.

## ADR-023 — Schedule OLED work after the v3 radio phases

2026-11-20. Keep dedicated v3 OLED and headless builds. Publish a read-only
v3 telemetry snapshot from validated application events. The v3 runtime
services the display only in a quiet tail, after radio and serial work,
and with no pending IRQ. Local client completion leaves peer DATA and
feedback windows protected. Clients without a usable beacon anchor listen
without display I/O. Provisioning can render with the radio inactive.

Require 10 ms for one display step plus half a radio guard before the next
beacon. Latch a display fault after a measured step over 10 ms. The gate
preserves the existing v3 calendar and packet format. The Wire timeout and
host tests do not establish a physical upper bound for a started call.

## ADR-024 — Keep one v3 firmware build target

2026-11-20. At the owner's request, retire the separate headless v3 ESP32
environment. The OLED environment inherits directly from the base Heltec
board and keeps both v3 and OLED build flags. Native headless and OLED tests
continue to exercise their respective runtime paths. Earlier build evidence
records the targets that existed when those checks ran.

## ADR-025 — Client OLED boot screen before beacon acquisition

2026-11-20. Give the v3 OLED Client up to 400 ms after successful radio
initialization to draw its first WAIT frame while the radio remains in
standby. The main loop continues to service serial commands. A ready
panel, a display error or deadline expiry releases the Client to listen.
After startup, the existing quiet-tail guard applies. This avoids starting
I²C during asynchronous beacon acquisition and does not change the
Gateway's beacon schedule.

## ADR-026 — Advance v3 OLED pages after a complete repaint

2026-11-20. Start the five-second page dwell once the panel has the complete
page. Draw the next title after its body and resume a split repaint at the
same position. Add 150 ms after the radio reservation for the automatically
derived OLED gateway period when the configured cap has room. The supplied
roster changes from 6,516 to 6,666 ms. Relative radio slots, guards, packet
layout and persisted configuration stay the same. Keep the existing IRQ,
I²C and next-beacon checks before each step.

## ADR-027 — Confirm bitmap transmission before opening gateway retry slots

2026-11-20. Keep the next receive mask clear until the radio reports
TX_DONE for the preceding feedback round. Apply the mask only in the
matching frame and retry round, by the first retry slot's start. Later
feedback can still carry cumulative acceptance. This change uses the
existing radio event and v3 wire format.

## ADR-028 — Keep v3 diagnostics available during beacon search

2026-11-20. Protect an active calendar and the next expected beacon RX
completion plus 25 ms. An expired calendar can no longer schedule Client
TX. Keep continuous RX during search and check for pending IRQs between
single OLED steps. A new valid beacon reinstates calendar protection.
The Gateway reserves possible peer activity after beacon or grant TX
errors, then gives the display the remaining tail. Leave the previous
session identity available for replay checks.

Use 250 ms of display reserve for the automatically derived OLED period
where the 10 s cap allows it. The bench period is 6,766 ms. Allow OLED
reset and redraw after a two-second fault backoff in a permitted window.
Once Wire is initialized, reset the panel without cycling the Vext rail
or reinstalling the bus. The v3 wire format still admits at most eight
clients. A 70-node design requires different capacity and control framing.

## ADR-030 — Separate radio completion time from slot launch time

2026-11-21. Latch the first DIO1 edge with the ESP timer and consume it
under a bounded cross-core critical section. The radio driver uses
poll-entry time when only the DIO1 level is available. V3 validates
receptions and anchors Beacon and Grant by completion time. Launch
checks use the current clock to keep expired slots silent. The
specified development roster is Gateway 1 and Clients 2–9.

## ADR-031 — Make multiclient the default and evaluate ADR offline

2026-11-21. The default v3 environment includes the board display. Keep
separate v1 and v2 compatibility builds. Log uplink evidence for each
granted client after the final bitmap completes. Use experiment-provided
thresholds and hysteresis for offline SF recommendations. Keep active
v3 on SF7 while control agreement, recovery and retune costs are open.

## ADR-032 — Per-frame adaptive DATA grants with a common control PHY

2026-11-22. Use an opt-in v4 wire version and SF9 control profile for
the walking experiment. Each Grant carries the DATA SF along with the
existing frame and Client boot identity. DATA and its retries use that SF.
Clients echo their last acknowledged frame in the next Request. Gateway
stability requires a receipt for a delivery at the same DATA SF, as well
as the first-slot delivery streak. The Client resynchronizes at SF9 and
uses volatile PHY state.
Keep v3 available for fixed-SF comparison and leave NVS schema 1 intact.

Reserve the full eight-Client roster's worst-case application lengths,
with four shared retry slots per round. Fund a higher SF before granting
it and keep its reservation during a lower-SF probe. Use an experimental
15 s frame, 80 ms guard, 20 ms retune lead and 300 ms quiet-tail reserve.
Measure the retune-to-RX path separately from packet delivery. Nominal
SNR thresholds and delivery streaks are trial inputs. Evaluate the
trade-off on one roaming Client while measuring stationary delivery.

## ADR-033 — Eight-Client bench load and timing evidence

Use a 16 B ceiling for the adaptive bench roster. The 8 B example fits
this traffic profile. At eight SF9 Clients the modeled frame leaves
0.916064 s after the 300 ms end reserve. The 24 B ceiling leaves 0.096864 s,
and the 64 B ceiling fits only two SF9 Clients with the rest at SF7.
Keep the 64 B codec limit, wire versions, NVS schema and PHY. The fixed-SF
image still advertises 64 B.

Preserve delivery and Rx time even when the Gateway reports a frame timing
fault. A completed bitmap must fall in its receive window and be serviced
before the next DATA round to open retries. Samples with timing faults
remain in captures and do not drive ADR. Keep cumulative feedback in all
four rounds so a Client can recover an earlier missed ACK. Measure the
overhead before experimenting with a different termination rule.

## ADR-034 — Observe DATA and separate scheduler faults from RF loss

Keep the adaptive field range at SF7–SF9. Report the last Client DATA
launch or valid Gateway DATA reception on each display page because the
radio normally returns to SF9 before the screen can refresh. When the
Gateway has no observation of RX readiness in the retune lead and receives
no valid packet in that slot, retain the delivery record and skip that
Client's ADR update. A Gateway-only comparison build freezes DATA at SF7
with the same control exchange and retry budget. Measure the matched pair
before attributing any delivery gain to ADR.

## ADR-035 — SF10 for budgeted DATA slots

Keep the SF9 control exchange and add SF10 to per-Client DATA grants.
SF10 at BW125 fits the existing airtime equation and TX deadline. The
Gateway still reserves the full roster and longest retry slots before
raising an SF. A move from SF10 to SF9 retains the SF10 reservation until
the trial locks. The provisional filtered SNR threshold for that trial is
−8.5 dB. Set the adaptive wire version to 5 so a mixed installation fails
at version validation before a legacy Client can see an incompatible
Grant. Use the adaptive image as the PlatformIO default for field work.

## ADR-036 — Recover the last verified SF after a walk outage

Release an inactive Client's higher-SF airtime after four frame IDs and
keep the Client session, last delivery identity and recovery SF in RAM.
The next valid Request can echo its last ACK, and the Gateway fits the
returning SF into the current reservations. A lower trial may lower that
recovery SF only after a lock. Gateway or Client session changes clear
the candidate.

Use two misses among the last three eligible reports at the same SF.
Successful retries count as first-attempt misses. A failed lower trial,
or six first attempts without a matching receipt, restores the prior SF.
Use the Client display to distinguish missing Beacon, missing Grant and
local TX failures during a walk. DATA still requires an SF9 Grant.

## ADR-037 — Use 32-byte synthetic DATA in the adaptive field profile

The sample generator sends a 32 B ASCII record with a node ID, sample
counter and fixed message. It advances the counter on a new queue entry,
so retries keep the original application bytes and packet identity.
Reserve 32 B for every adaptive roster position. Keep the 15 s period,
the PHY and the existing wire and NVS formats. An earlier Gateway that
advertises a 16 B ceiling cannot grant the new sample.

At 32 B, five SF9 Clients or two SF10 Clients fit alongside peers at SF7.
Keep the full-roster admission check and verify the tighter margins on
real boards before extending the active schedule.

## ADR-038 — Separate loss evidence from SNR margin

Keep Legacy as the default. The Gateway can select EWMA or local-trend
Kalman at build time. Both use valid Request SNR on a common scale and
accepted DATA SNR as a fallback. Preserve the estimate across SF changes.
Reset it on a new session or idle state expiry. Keep the validated
Request, Grant, DATA, ACK, airtime admission and rollback paths.

A same-SF retry rescue contributes less evidence than complete loss.
Four consecutive complete losses can request a higher SF even with
healthy margin. Defer a common-mode filter until measurements can
distinguish burst interference from simultaneous fades. Treat the
thresholds and the simulator channel as hypotheses for field comparison.

## ADR-039 — Qualify complete DATA loss and bound trial lifetime

2026-11-23. Supersedes the strong-margin override in ADR-038. A valid
Request establishes uplink control reception but cannot confirm Grant
delivery or DATA TX. In every rule, count a complete miss toward an
increase only with a finite Request margin below the upshift threshold.
A same-SF retry rescue remains a separate observation. Legacy stays
the default, and every increase must fit the full roster.

Track a lower-SF trial by frame ID regardless of the success-counter cap
or availability of usable reports. Allow receipt through the deadline
frame. Roll back a degraded trial sooner and release an inactive Client's
reservation through the existing lease rule.

Use parameter revision 2. Check score reachability, positive hysteresis,
effective threshold order and estimator inputs. Keep the current
numerical tuning pending field calibration. Use a fixed independent
decode curve when testing model sensitivity to a policy threshold,
and start each blockage trial at the same SF with fresh state.
Wire v5, PHY and NVS schema stay compatible.
