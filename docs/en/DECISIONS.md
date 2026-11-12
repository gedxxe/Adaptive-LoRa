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
