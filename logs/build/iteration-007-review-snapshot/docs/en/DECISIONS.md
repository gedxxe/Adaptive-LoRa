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
