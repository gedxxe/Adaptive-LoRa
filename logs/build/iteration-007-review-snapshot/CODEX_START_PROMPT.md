# Codex Task — Implement M0–M2 of Adaptive-LoRa Firmware

Read `AGENTS.md` and all canonical English documents under `docs/en/`
before editing. The documentation is the project contract.

## Hardware available

There are only two physical units:

1.  Gateway unit: ESP32-C3 Super Mini + Ai-Thinker Ra-01SCH (LLCC68).
2.  Client unit: ESP32-C3 Super Mini + Ai-Thinker Ra-01SCH (LLCC68).

Both units have the same hardware. The antenna under test is a 915 MHz
Moxon.

**Important:** Ra-01SCH is LLCC68. Use RadioLib `LLCC68`, not `SX1262`.

## Required architecture

Use one codebase and one firmware image. Do not create separate gateway
and client repositories or duplicate source trees.

At boot:

- Load validated device configuration from NVS.
- If role is `GATEWAY`, run `GatewayApp`.
- If role is `CLIENT`, run `ClientApp`.
- If role is missing, corrupt, or unsupported, run provisioning mode
  with radio TX disabled.

Implement the serial provisioning commands defined in `README.md` and
`docs/en/ARCHITECTURE.md`.

Do not attempt to infer the role from hardware; the two devices are
identical.

Both application modules must exist:

- `src/gateway/gateway_app.h/.cpp`
- `src/client/client_app.h/.cpp`

`src/main.cpp` must remain a thin entry point. No radio calls, frame
parsing, state machine, retry policy, or protocol handlers belong in
`main.cpp`.

## First delivery: M0–M2

1.  Inspect the existing workspace and preserve existing work.
2.  Create or reconcile the repository structure documented in
    `AGENTS.md`.
3.  Add `platformio.ini` with one ESP32-C3 firmware environment and one
    native host-test environment.
4.  Pin RadioLib to `jgromes/RadioLib@7.8.1`.
5.  Confirm actual board/module pin labels from available hardware
    evidence. If not verifiable, keep the pin map marked provisional and
    report the exact items requiring physical confirmation.
6.  Initialize the radio with RadioLib `LLCC68`.
7.  Implement validated NVS configuration and the bounded serial
    provisioning console.
8.  Implement explicit, bounded frame encoding/decoding using the
    canonical v1 frame layout in `docs/en/PROTOCOL.md`.
9.  Implement client `HELLO` transmission at a configurable interval.
10. Implement gateway frame validation and `ACK` response.
11. ACK must echo the original client source ID, boot ID, and sequence
    in its payload.
12. Client must reject an ACK that does not match the outstanding HELLO
    identity.
13. Handle duplicate HELLO frames idempotently.
14. Log radio status, RSSI, SNR, SF, BW, frequency, sequence, and event
    result.
15. Add native tests for valid frames, malformed frames, length
    boundaries, wrong version/type, role config validation, and
    duplicate packet identity.
16. Build the ESP32 environment and run native tests.
17. Update the documentation memory files as required below.

## Initial PHY

Use the provisional values from `AGENTS.md` and `docs/en/HARDWARE.md`:

- Frequency: 915.0 MHz (provisional bench setting; check local rules
  before deployment).
- BW: 125 kHz.
- Control SF: SF9.
- Initial data SF: SF7.
- CR: 4/5.
- Preamble: 8.
- Explicit header.
- PHY CRC enabled.
- One common private sync word, centralized in configuration.
- Initial TX power: 10 dBm.
- SPI clock: 1 MHz.

**At BW125, LLCC68 supports SF5–SF9 only. Never configure SF10, SF11, or
SF12.**

Do not dynamically change SF in M0–M2.

## Scope exclusions

Do not add:

- TDMA or multi-client scheduling.
- ACK bitmap or selective retransmission.
- ADR or dynamic PHY configuration.
- Wi-Fi, Modbus, RS485, PostgreSQL, Neon, Vercel, or dashboard
  integration.
- LoRaWAN, SX1302, OTA, or unrelated dependencies.

After M2 is complete, provide an implementation plan for M3–M8. Do not
start those milestones in the same change unless explicitly instructed.

## Mandatory documentation memory

At the end of **every** coding/build iteration, whether successful or
failed:

1.  Append an iteration entry to `docs/en/BUILD_MEMORY.md`.
2.  Refresh the current snapshot in `docs/en/PROGRESS.md`.
3.  Add every build/test/hardware error to `docs/en/ERROR_LOG.md`, with
    reproduction and status.
4.  Append an ADR to `docs/en/DECISIONS.md` if a technical decision
    changed.
5.  Update `CHANGELOG.md` if firmware behavior or a release item
    changed.
6.  Update Chinese documentation when a technical decision or operator
    procedure changed.
7.  Save full command output under `logs/build/` or `logs/test/` when
    useful and reference the exact path.

Never overwrite previous iteration entries. Do not invent commands,
outputs, versions, measurements, or test results. Use `NOT MEASURED` or
`TBD` where evidence is unavailable.

## Final report

Report:

- Exact files created/modified.
- Responsibility of every `.cpp` file.
- Explicit confirmation that `main.cpp` is thin.
- ESP32 build command, exit code, and result.
- Native test command, test count, and result.
- RadioLib and resolved toolchain versions.
- NVS provisioning test results.
- Hardware checks still unverified.
- Two-device serial test procedure.
- Current milestone status and next milestone acceptance criteria.
- Updated paths for build memory and error logs.

Use concise technical English. Do not use marketing language,
unsupported performance claims, or generic AI-generated filler. Do not
create commits unless explicitly requested.
