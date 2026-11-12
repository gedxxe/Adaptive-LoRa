# Milestone Task Plan for Codex

Do not implement all milestones in one large change. Complete one
milestone, record evidence, and stop for review.

## M0 — Hardware and toolchain baseline

- Inspect both Super Mini boards and both Ra-01SCH modules.
- Confirm board/module revisions and pin orientation.
- Verify GPIO continuity against `HARDWARE.md`.
- Verify radio 3.3 V supply and antenna connection.
- Confirm local RF rules for the intended frequency and power.
- Record toolchain versions.
- Do not transmit until antenna and supply are verified.

## M1 — LLCC68 bring-up and role provisioning

- Create the single PlatformIO firmware environment.
- Pin RadioLib 7.8.1.
- Implement the pure `DeviceConfig` model/validator and the separate
  `NvsConfigStore` persistence wrapper.
- Implement bounded serial provisioning commands.
- Implement `FirmwareRuntime` role dispatch.
- Implement `RadioDriver` with a single LLCC68 owner.
- Test valid, missing, corrupt, and incompatible configuration.
- Verify invalid role enters safe provisioning mode with TX disabled.

## M2 — Fixed-PHY HELLO/ACK

- Implement protocol v1 codec.
- Add native codec and sequence/deduplication tests.
- Implement `ClientApp` HELLO state machine.
- Implement `GatewayApp` receive/validate/ACK state machine.
- Validate ACK echoed source/boot/sequence.
- Test timeout and duplicate HELLO.
- Perform a short-range two-device RF run.
- Update all memory files.

## M3 — Beacon and one-client TDMA

- Add `common/scheduler/` only now.
- Define beacon payload and relative timing reference.
- Add frame ID and schedule version.
- Implement client synchronization and slot state.
- Measure radio switching, IRQ timing, and clock drift.
- Add measured guard time.
- Test missed beacon and stale schedule.
- Do not generalize to 70 clients in this milestone.

## M4 — ACK bitmap and selective retransmission

- Add schedule-indexed bitmap encoding/decoding.
- Test 1, 10, and 70 client bitmaps.
- Add deterministic retry slots.
- Add `MISSING` and `ACK_UNKNOWN` states.
- Reuse packet identity on retry.
- Test dropped DATA, dropped ACK bitmap, and duplicates.
- Verify no duplicate application delivery.

## M5 — Conservative ADR

- Add gateway-side per-client metric windows.
- Centralize thresholds and label them experimental.
- Add hysteresis.
- Implement CONFIG_PROPOSE / ACCEPT / COMMIT.
- Apply only at effective frame boundaries.
- Keep BW125 and SF7–SF9.
- Test lost handshake messages and rollback behavior.
- Do not adjust TX power in this release.

## M6 — Synchronization recovery and rejoin

- Define beacon timeout and schedule lease.
- Stop normal DATA when synchronization expires.
- Implement REJOIN_REQUEST / REJOIN_GRANT.
- Test gateway/client reboot and stale traffic.
- Record recovery time and failure modes.

## M7 — Controlled Moxon evaluation

- Define a repeatable RF test route and positions.
- Use identical PHY, power, packet size, and test duration.
- Compare the same reference antenna and Moxon where possible.
- Record PDR, RSSI, SNR, retries, latency, and environment.
- Do not state antenna gain without measurement.

## M8 — Multi-node simulation and preparation

- Simulate 5, 10, 20, 35, and 70 logical clients.
- Include airtime, switching, guard, ACK, and retry costs.
- Report channel utilization and schedule feasibility.
- Clearly label results as simulation.
- Prepare physical multi-node testing only when additional hardware is
  available.
