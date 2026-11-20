# Milestone Task Plan for Codex

Do not implement all milestones in one large change. Complete one
milestone, record evidence, and stop for review.

Adaptive-LoRa research title: Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks. This is the research direction; firmware 0.2.0-alpha.1 remains the fixed-PHY M0–M2 baseline.

## Integration boundary work — separately reviewed, not implemented here

- Preserve pure encoding/config/identity helpers and the existing radio transport contract.
- Before embedding in another application, specify payload submission/delivery, link events and statistics without meter/backhaul/database assumptions.
- Design clock/random/persistence/interrupt service boundaries from concrete portability requirements; current ESP32 calls and native test shims are not a general production platform layer.
- Extend radio lifecycle/PHY interfaces only when a second verified adapter is needed. The single SX1262 adapter is implemented; add another only for a concrete requirement.
- Do not start this API work, scheduling, bitmap/retry or ADR merely as part of identity/publication edits.

## M0 — Hardware and toolchain baseline

- Inspect both V3.4-marked Heltec boards and confirm the wiring and RF variant.
- Confirm board/module revisions and pin orientation.
- Verify GPIO continuity against `HARDWARE.md`.
- Verify radio 3.3 V supply and antenna connection.
- Confirm local RF rules for the intended frequency and power.
- Record toolchain versions.
- Do not transmit until antenna and supply are verified.

## M1 — SX1262 bring-up and role provisioning

- Create the single PlatformIO firmware environment.
- Pin RadioLib 7.8.1.
- Implement the pure `DeviceConfig` model/validator and the separate
  `NvsConfigStore` persistence wrapper.
- Implement bounded serial provisioning commands.
- Implement `FirmwareRuntime` role dispatch.
- Retain `RadioDriver` policy and the sole Sx1262Hardware owner; verify the integrated radio physically.
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

The opt-in v2 implementation and native scenarios are in Iteration 019.
Verify the serialized slot calendar on two boards. Measure IRQ service,
TX launch and RX re-arm delay, clock drift and late/missed beacon recovery.
Set guard margins from those measurements before expanding the schedule.

## M4 — ACK bitmap and selective retransmission

Iteration 019 implements one-client bitmap feedback and three scheduled
retries, preserving DATA identity. Format checks cover 1, 10 and 70 logical
positions. Verify lost DATA, lost feedback, duplicate suppression and retry
exhaustion on the actual link. Review the multi-client schedule and bitmap
binding before expanding the wire contract.

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

Iteration014 completed the single-target S3/SX1262 source migration and host checks. M0-M2 still need physical acceptance under the current runbook. Procurement/backup/terminated RF tests precede the planned five-client pilot;70-node work remains a separate research study.


## Iteration 015 planning checkpoint

M0–M2 software regressions pass; physical acceptance is still IN PROGRESS.
M3–M8 production remains NOT STARTED. The host-only feasibility model is
preliminary research, not completion of M3 or M8. See TDMA_FEASIBILITY.md for
the candidate one-entry beacon, cached-schedule limits, 64-bit anchors,
unknown timing inputs and per-active-client deduplication recommendation.

Before M3 production changes: collect current-board M0/M1/M2 evidence and
ERR-021 disposition, measure radio preparation/IRQ/processing/drift, review
slot/guard bounds, freeze a one-client beacon plus compatibility/version
policy, then test synchronization, missed beacon, stale schedule, session
change and timer wrap. Do not activate reserved types in this iteration.
Before M4: define admission/session eviction and reboot semantics, schedule
mapping and complete bitmap validation; preserve identity/payload on retry.
A 70-entry mapping does not fit one current 96-byte beacon, and analytical
arithmetic does not replace the separate M8 scaling study.
