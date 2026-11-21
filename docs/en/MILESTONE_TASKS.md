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

## M3 — Beacon and bounded TDMA

Iteration 019 implements a v2 single-client link. Iteration 020 adds v3
exact-length grants and exercises up to eight clients on the host. Verify
the serialized slot calendar on two boards. Measure IRQ service,
TX launch and RX re-arm delay, clock drift and late/missed beacon recovery.
Set guard margins from those measurements before expanding the schedule.
Iteration 021 anchors v3 to DIO1 completion and keeps launch decisions
on current time. The development roster is Gateway 1 and Clients 2–9.

## M4 — ACK bitmap and selective retransmission

The v3 gateway sends an aggregate bitmap and up to four shared retry grants
after each round. Verify lost DATA, lost feedback, duplicate suppression and
retry exhaustion on the actual link. A ten-client wire extension requires
separate review and physical evidence for the guard.

## M5 — Conservative ADR

- Use the gateway's per-client evidence and offline analysis for initial
  metric windows, threshold selection and hysteresis.
- Measure retune, turnaround and oscillator drift with the development roster.
- Define versioned CONFIG_PROPOSE / ACCEPT / COMMIT and a bounded
  lease on the common control profile. A client without a matching
  grant stays silent. The gateway returns to common control for the next beacon.
- Apply only at announced frame boundaries. Size each serialized PHY block
  from airtime, retune, RX re-arm, feedback and drift. Reject plans that
  exceed the roster budget.
- Keep BW125 and SF7–SF9.
- Test loss of each handshake message, inconsistent versions, resets,
  rollback, and that all eight clients can still rejoin on the common profile.
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
