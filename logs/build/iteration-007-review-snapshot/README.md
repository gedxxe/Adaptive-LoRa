# Adaptive-LoRa Firmware

A staged firmware project for a private LoRa P2P link using ESP32-C3
Super Mini boards and Ai-Thinker Ra-01SCH (LLCC68) modules.

## Current inventory and intended setup

| Device | Hardware                       | Intended role   |
|--------|--------------------------------|-----------------|
| Unit A | ESP32-C3 Super Mini + Ra-01SCH | Gateway         |
| Unit B | ESP32-C3 Super Mini + Ra-01SCH | Client          |

The table is the intended pair, not assembled hardware. Current stock:
**2 RA-01SCH/LLCC68 modules, 1 ESP32-C3 Super Mini**. The second MCU has
not been purchased and the 915 MHz Moxon has not been fabricated.
Work is temporarily limited to software verification/protocol hardening
for M0–M2; physical NVS and RF tests have not been performed.

The same firmware image is used on both devices. The role is stored in
NVS and loaded at boot. A device without a valid role enters serial
provisioning mode and does not transmit.

The planned antenna is a 915 MHz Moxon. Frequency, power, and
antenna operation remain subject to local regulatory and RF validation.

## Important hardware distinction

The module is **LLCC68**, not SX1262. Use RadioLib `LLCC68`. Do not copy
the Heltec SX1262 wiring or driver configuration from another prototype.

## What this package contains

- `AGENTS.md`: mandatory Codex engineering rules and project
  constraints.
- `CODEX_START_PROMPT.md`: first implementation task for Codex.
- `platformio.ini`: one firmware environment and one native test environment.
- `src/`, `include/`, `test/`: M0–M2 implementation and native tests.
- `docs/en/BENCH_RUNBOOK.md`: hardware gates, provisioning and two-port capture procedure.
- `docs/en/`: canonical engineering documentation.
- `docs/zh-CN/`: Simplified Chinese companion documentation.
- `docs/en/BUILD_MEMORY.md`: append-only record of every coding/build
  iteration.
- `docs/en/ERROR_LOG.md`: issue tracking with reproduction and
  verification evidence.
- `docs/en/DECISIONS.md`: architecture and protocol decision record.
- `logs/`: location for full build and test command output.

## First implementation target

Implement M0–M2 only:

1.  Verify the actual board, module, pin map, power rail, and antenna
    connection.
2.  Initialize LLCC68 on both devices.
3.  Implement serial role provisioning and NVS configuration.
4.  Implement fixed-PHY `HELLO` / `ACK`.
5.  Add native tests for frame codec, malformed frames, role
    configuration, and duplicate handling.
6.  Record exact build and test evidence.

Do not implement full TDMA, bitmap ACK, retransmission scheduling, or
ADR in the first delivery.

## Build commands

From the repository root:

``` bash
pio run -e esp32c3_supermini
pio test -e native
pio run -e esp32c3_supermini -t upload
pio device monitor -b 115200
```

Candidate `0.2.0-alpha.1`: ESP32 build passed (exit 0); **92 native tests passed** (exit 0), including 38 scenarios using the actual production client/gateway state machines through a bounded fake transport. See `docs/en/SOFTWARE_AUDIT.md` and `docs/en/BUILD_MEMORY.md`. These host loss/delay/reboot scenarios are simulated, not physical NVS or RF validation. M0–M2 remain IN PROGRESS; no upload or physical test has been performed.

This workstation resolves `espressif32` to installed pioarduino 55.03.311 / Arduino-ESP32 3.3.11. See the recorded package log before reproducing on a different workstation. RadioLib remains exactly 7.8.1. Native USB CDC is a provisional board assumption; verify the actual USB path.

The `esp32-c3-devkitm-1` PlatformIO board target is a generic starting
point. Verify the exact Super Mini board revision, flash size, and USB
serial behavior before treating the configuration as final.

## Provisioning workflow

After the first flash, open the serial monitor at 115200 baud.

Example for the gateway:

``` text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config save
reboot
```

Example for the client:

``` text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config save
reboot
```

`config show` displays staged edits; `status` displays the loaded boot config and validity. `config save` validates and verifies NVS readback; changes activate only after `reboot`. Role edits preserve IDs, so explicitly set all IDs as above. Missing/corrupt/incompatible NVS leaves the radio uninitialized and TX disabled.

Optional: `config set hello-interval-ms <1000..3600000>`; default 5000 ms between a transaction result and the next HELLO. Save and reboot to apply. `help` lists commands. Network ID 65535 is valid; node/gateway IDs 0 and 65535 are reserved.

Use [the bench runbook](docs/en/BENCH_RUNBOOK.md) and [physical test record template](logs/test/RUN_RECORD_TEMPLATE.md) for acceptance. Only Bluetooth serial ports were enumerated on this workstation during implementation; no ESP32 serial port was identified.

## Codex workflow

Keep gateway, client, shared protocol, and radio-driver work in the same
repository and preferably the same Codex workspace/conversation. Use
separate milestone prompts when needed, but do not split the shared
protocol into independently evolving repositories. Parallel work is
appropriate only when file ownership is clear and shared interfaces are
agreed.

## Development principles

- One codebase and one firmware image.
- Explicit role selection through NVS; no hardware role guessing.
- Thin `main.cpp`; separate gateway and client application modules.
- One owner for all radio/SPI operations.
- Fixed-size buffers and explicit frame serialization.
- Host-testable protocol code independent of Arduino/RadioLib.
- No unsupported performance claims.
- Every build iteration is recorded, including failed iterations.

Read `AGENTS.md` before changing code.
