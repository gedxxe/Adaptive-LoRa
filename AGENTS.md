# AGENTS.md — Adaptive-LoRa Research Firmware

## 1. Project purpose

Develop Adaptive-LoRa as an independent research and firmware project.

Research title: Reliability-aware TDMA Scheduling and Link Adaptation for
Single-Radio LoRa Industrial Energy Monitoring Networks.

Industrial energy monitoring is a general research application domain,
not a deployment-specific protocol dependency. The current firmware is
the fixed-PHY M0–M2 HELLO/ACK baseline; scheduling and adaptation remain
future milestones. No deployment customer or internal system identity
may appear in publication content.

The planned first physical test setup contains exactly two radio devices:

| Role    | MCU                 | LoRa module                 | Quantity |
|---------|---------------------|-----------------------------|---------:|
| Gateway | ESP32-C3 Super Mini | Ai-Thinker Ra-01SCH, LLCC68 |        1 |
| Client  | ESP32-C3 Super Mini | Ai-Thinker Ra-01SCH, LLCC68 |        1 |

Current inventory (user correction, 2026-11-12): two RA-01SCH/LLCC68
modules, one ESP32-C3 Super Mini, second MCU not yet purchased, and
915 MHz Moxon not yet fabricated. No two-device setup is assembled.
Current work is software verification and M0–M2 protocol hardening.

Both planned devices use the same hardware. The planned antenna is a 915 MHz
Moxon. The first physical objective is to establish a repeatable two-device
radio link, then introduce gateway-scheduled communication in measured
stages.

This is a custom LoRa P2P MAC/protocol. It is **not LoRaWAN**.

### Scope boundary for the first firmware phase

Implement only:

- LLCC68 radio bring-up.
- Persistent device-role configuration.
- Serial provisioning and diagnostics.
- Fixed-PHY `HELLO` / `ACK`.
- Bounded frame encoding/decoding.
- Host unit tests and repeatable hardware test records.

Do not add the following during M0–M2:

- Modbus or RS485 meter acquisition.
- Wi-Fi, Ethernet, cellular backhaul, WebSocket, MQTT, or HTTP.
- PostgreSQL, Neon, Vercel, or dashboard integration.
- LoRaWAN, SX1302, LoRaWAN gateways, or LoRaWAN servers.
- OTA updates.
- Multi-SF concurrent reception.
- Full TDMA, ACK bitmap, retransmission scheduler, or ADR before the
  relevant milestone.

Keep later integration possible through clean application boundaries,
but do not implement it prematurely.

## 2. Mandatory architecture decision: one firmware, selectable role

Gateway and client share the same PCB and radio module. Do not maintain
two separate firmware codebases.

Build **one firmware image**. At boot, the firmware reads the configured
role from ESP32 NVS:

- `GATEWAY` → instantiate and run `GatewayApp`.
- `CLIENT` → instantiate and run `ClientApp`.
- Missing, invalid, or incompatible configuration → enter
  `PROVISIONING MODE`.

There is no reliable hardware-based way to infer the intended role
because the two devices are physically identical. Do not claim automatic
hardware detection. Role selection is explicit provisioning followed by
automatic role loading at every boot.

### Provisioning requirements

Provide a small serial command interface at 115200 baud. The minimum
command set is:

``` text
help
status
config show
config set role gateway|client
config set network-id <1..65535>
config set node-id <1..65534>
config set gateway-id <1..65534>
config save
reboot
```

Rules:

- `config save` validates the complete configuration before persisting
  it.
- For `GATEWAY`, `node-id` must equal `gateway-id` because the gateway
  identifies itself with that ID.
- For `CLIENT`, `node-id` must differ from `gateway-id`.
- ID `0` is reserved; `65535` is reserved for broadcast where
  applicable.
- The gateway uses node ID `1` and the initial client uses node ID `2`
  unless explicitly configured otherwise.
- Initial network ID is `1`.
- A device with no valid saved role must not transmit application
  packets.
- Invalid NVS data must not silently default to gateway or client.
- A role change takes effect after a controlled reboot; do not switch
  roles in the middle of an active radio transaction.
- Show the loaded role, IDs, config schema version, and config validity
  in `status`.
- Keep the provisioning parser bounded. Do not use unbounded dynamic
  `String` concatenation.

NVS is configuration storage, not a packet log. Do not write radio
telemetry to NVS on every packet.

## 3. Read-before-edit requirements

Before changing code, inspect the existing workspace and read:

1.  `README.md`
2.  `docs/en/ARCHITECTURE.md`
3.  `docs/en/HARDWARE.md`
4.  `docs/en/PROTOCOL.md`
5.  `docs/en/TEST_PLAN.md`
6.  `docs/en/PROGRESS.md`
7.  `docs/en/BUILD_MEMORY.md`
8.  `docs/en/ERROR_LOG.md`
9.  `docs/en/DECISIONS.md`
10. `docs/en/VERSIONING.md`

English is the canonical engineering record. Keep the corresponding
`docs/zh-CN/` documents aligned whenever a technical decision or
user-facing operating procedure changes.

Do not assume a file is absent until the workspace has been inspected.
Preserve existing work. Do not replace a populated file with a generic
template.

## 4. Hardware facts and constraints

The installed module is **Ai-Thinker Ra-01SCH based on LLCC68**. It is
not the Heltec SX1262 board used in a different prototype.

Use RadioLib’s `LLCC68` class. Do not instantiate `SX1262`.

Manufacturer information recorded for this project:

- Frequency range: 803–930 MHz.
- Supply range: 2.7–3.6 V; 3.3 V nominal.
- RF output capability is specified up to +22 dBm under suitable supply
  and operating conditions; this does not authorize using that power.
- SPI interface.
- BUSY and DIO1 must be connected to MCU GPIO.
- The standard Ra-01SCH module handles its RF switch internally;
  TXEN/RXEN are not normally wired as independent MCU controls.
- Packet engine limit: up to 256 bytes.
- LLCC68 spreading-factor limits:
  - BW125 kHz: SF5–SF9.
  - BW250 kHz: SF5–SF10.
  - BW500 kHz: SF5–SF11.
- **SF12 is not supported.**
- Do not assume every SX1262 feature or PHY combination is available on
  LLCC68.

The 915 MHz Moxon is within the module’s stated frequency range. That
fact does not establish antenna matching, legal channel availability,
regulatory compliance, or link performance. Confirm the locally
permitted frequency, channel, EIRP, duty-cycle/access requirements, and
equipment requirements before operational deployment.

### Proposed pin map — verify before wiring

This map is a starting proposal, not a confirmed board schematic.
ESP32-C3 Super Mini clones and Ra-01SCH carrier boards can differ.
Inspect the actual silkscreen, board revision, module pin labels, and
continuity before connecting power.

|            ESP32-C3 GPIO | Ra-01SCH pin | Signal   |
|-------------------------:|-------------:|----------|
|                    GPIO4 |           12 | SCK      |
|                    GPIO5 |           13 | MISO     |
|                    GPIO6 |           14 | MOSI     |
|                    GPIO7 |           15 | NSS / CS |
|                    GPIO1 |            4 | RESET    |
|                    GPIO0 |            6 | DIO1     |
|                   GPIO10 |           10 | BUSY     |
| External regulated 3.3 V |            3 | VCC      |
|                      GND |     2, 9, 16 | Ground   |
|      50-ohm antenna feed |            1 | ANT      |

Ra-01SCH pins 5 (TXEN) and 11 (RXEN) are RF-control pins. Leave them
unconnected unless the exact module/breakout documentation explicitly
requires external control. DIO2/DIO3 are not needed for the first
implementation.

ESP32-C3 cautions:

- GPIO2, GPIO8, and GPIO9 are strapping pins; avoid them for radio
  control.
- GPIO18/19 are associated with USB Serial/JTAG.
- GPIO20/21 are UART0.
- GPIO4–GPIO7 can overlap external JTAG functions. The proposed map uses
  them for SPI; external JTAG may require another mapping.
- Confirm whether the selected Super Mini exposes the proposed GPIOs and
  whether its USB serial path is native USB or a USB-UART bridge.
- If a pin differs, update `include/board_pins.h` and
  `docs/en/HARDWARE.md` together. Do not silently change pins in source
  only.

### Power integrity

- Supply the radio within 2.7–3.6 V; target a stable 3.3 V rail.
- Do not assume the Super Mini 3V3 pin can supply radio TX transients.
- Prefer a separate regulated 3.3 V radio rail rated for at least 300
  mA, with additional margin after checking the actual module and
  regulator datasheets.
- Join grounds between ESP32 and radio.
- Place local decoupling close to the radio module.
- Measure the radio supply during TX at the selected power.
- Never apply 5 V to the radio VCC.
- Never transmit with an open antenna port.
- Start at 10 dBm. Do not increase power to solve a link problem before
  checking the supply, antenna, matching, placement, and legal limits.

## 5. Software baseline

Use PlatformIO with the Arduino framework for the first two-device
prototype.

- Platform: `espressif32`.
- Generic board target: `esp32-c3-devkitm-1`, subject to checking the
  actual Super Mini flash size, USB behavior, and board revision.
- C++17 where supported by the resolved toolchain.
- Radio library: `jgromes/RadioLib@7.8.1`.
- Radio class: `LLCC68`.
- Pin the dependency. Do not upgrade to RadioLib 8.x without a separate
  migration task and API review.
- Record actual PlatformIO Core, platform, Arduino-ESP32, compiler, and
  RadioLib versions after the first successful build.
- RadioLib is used for PHY control only. Implement the Adaptive-LoRa MAC and
  application protocol in this repository.

Do not invent toolchain versions or report a build as successful unless
the command was executed and its exit status was checked.

## 6. Initial PHY profile

All values are provisional bench settings. Keep them in one
configuration header; do not scatter constants through application code.

| Parameter         |                     Initial value | Status                                 |
|-------------------|----------------------------------:|----------------------------------------|
| Frequency         |                         915.0 MHz | Provisional; regulatory check required |
| Bandwidth         |                           125 kHz | Initial fixed setting                  |
| Control SF        |                               SF9 | Provisional; verify link               |
| Initial data SF   |                               SF7 | Provisional                            |
| Initial ADR range |                           SF7–SF9 | Later milestone only                   |
| Coding rate       |                               4/5 | Initial                                |
| Preamble          |                         8 symbols | Initial                                |
| Header            |                          Explicit | Initial                                |
| PHY CRC           |                           Enabled | Required                               |
| Sync word         | One common private LoRa sync word | Value to be centralized and verified   |
| TX power          |                            10 dBm | Initial bench setting                  |
| SPI clock         |                             1 MHz | Initial conservative setting           |

At BW125 kHz, do not configure SF10, SF11, or SF12. Do not dynamically
change bandwidth in the first ADR implementation. Keep BW125 and
evaluate SF7/SF8/SF9 only.

Control and data are logical planes that time-share one LLCC68. They are
not separate radios.

## 7. Repository structure and module ownership

Keep the repository lean and explicit:

``` text
Adaptive-LoRa/
├── AGENTS.md
├── README.md
├── README.zh-CN.md
├── CODEX_START_PROMPT.md
├── CHANGELOG.md
├── VERSION
├── PROTOCOL_VERSION
├── platformio.ini
├── include/
│   ├── board_pins.h
│   ├── project_config.h
│   └── radio_config.h
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── firmware_runtime.h
│   │   ├── firmware_runtime.cpp
│   │   ├── nvs_config_store.h
│   │   ├── nvs_config_store.cpp
│   │   ├── provisioning_console.h
│   │   └── provisioning_console.cpp
│   ├── common/
│   │   ├── config/
│   │   │   ├── device_config.h
│   │   │   └── device_config.cpp
│   │   ├── radio/
│   │   │   ├── radio_driver.h
│   │   │   └── radio_driver.cpp
│   │   ├── protocol/
│   │   │   ├── message_types.h
│   │   │   ├── frame_codec.h
│   │   │   ├── frame_codec.cpp
│   │   │   ├── sequence_tracker.h
│   │   │   └── sequence_tracker.cpp
│   │   └── diagnostics/
│   │       ├── serial_logger.h
│   │       └── serial_logger.cpp
│   ├── gateway/
│   │   ├── gateway_app.h
│   │   └── gateway_app.cpp
│   └── client/
│       ├── client_app.h
│       └── client_app.cpp
├── test/
│   ├── test_frame_codec/
│   ├── test_role_config/
│   └── test_sequence_tracker/
├── docs/
│   ├── en/
│   └── zh-CN/
└── logs/
    ├── build/
    └── test/
```

Add `common/scheduler/`, `common/adr/`, `common/storage/`, or additional
protocol modules only when the milestone needs them. Do not create a
framework for hypothetical features.

### Non-monolithic rule — mandatory

- `src/main.cpp` must be a thin Arduino entry point. It may construct
  the runtime and delegate `setup()` / `loop()`; it must not contain
  application logic.
- `FirmwareRuntime` loads configuration, selects the operating mode, and
  delegates to `GatewayApp`, `ClientApp`, or provisioning mode. It receives
  the existing ConfigStorage/RadioHardware references; Esp32Firmware owns
  the concrete adapters before and beyond the coordinator lifetime. Main
  constructs only that composition and delegates setup/loop.
- `GatewayApp` owns gateway application state transitions.
- `ClientApp` owns client application state transitions.
- `DeviceConfig` is a pure configuration model and validator,
  independent of Arduino.
- `NvsConfigStore` owns configuration load/save/verification policy through
  a non-owning `ConfigStorage` reference. `PreferencesConfigStorage` solely
  owns Preferences/NVS access; native storage tests execute the actual store
  using low-level responses, never a duplicate validation/store model.
- `ProvisioningConsole` owns serial command parsing.
- The radio module has one `RadioDriver` state owner and one injected
  `Llcc68Hardware` adapter. Only the adapter owns Module/LLCC68 and
  performs RadioLib/SPI/GPIO operations. Native tests execute the actual
  driver through RadioHardware; do not duplicate transition logic.
- `FrameCodec` is independent of Arduino and RadioLib.
- `SequenceTracker` handles bounded sequence/deduplication state.
- Diagnostics code formats logs; it must not operate the radio.
- Do not put packet parsing, RadioLib calls, TX/RX loops, retry logic,
  ADR decisions, or large switch statements in `main.cpp`.
- Keep `main.cpp` approximately 15–40 lines as a review guideline, not a
  formatting contest.
- Avoid unnecessary factories, dependency-injection frameworks,
  inheritance trees, and one-class-per-message wrappers.
- Do not instantiate a second radio object in an application module.

## 8. Radio ownership and concurrency

Use exactly one owner for all LLCC68 and SPI operations.

For the first implementation, prefer one event-driven main loop:

1.  Poll or consume radio events.
2.  Process a bounded event.
3.  Advance the selected application state machine.
4.  Service serial provisioning/diagnostics.
5.  Return without blocking for long intervals.

If FreeRTOS tasks are introduced later, retain one radio-owner task and
communicate through bounded queues.

ISR/DIO callbacks may only set a `volatile`/atomic event flag or enqueue
a minimal event using an ISR-safe primitive. They must not:

- call SPI or RadioLib;
- parse packets;
- allocate memory;
- format or print logs;
- execute application state transitions.

Use monotonic microsecond timing for slot deadlines. Use wrap-safe time
comparisons. Measure radio reconfiguration and TX/RX transition time
before selecting TDMA guard intervals.

## 9. Protocol requirements

### Binary frame rules

- Use explicit byte serialization and explicit endianness.
- Never transmit raw C++ structs or rely on compiler packing/alignment.
- Use fixed-size buffers and strict bounds checks.
- Start with a maximum complete application frame of 96 bytes, including
  the header.
- The LLCC68 packet engine limit is not the application protocol target.
- PHY CRC is enabled. Do not add an application checksum unless a
  concrete requirement is documented.
- Reject unknown protocol versions, unknown message types, malformed
  lengths, wrong network IDs, and invalid destinations.
- Codec and pure configuration-validation code must compile in the
  native host-test environment without Arduino or RadioLib.

The canonical v1 header layout is specified in `docs/en/PROTOCOL.md`. Do
not change field order, width, or endianness without updating the
protocol document and versioning record.

### Deduplication

Use the logical packet identity:

`network_id + source_id + boot_id + sequence`

A retry reuses the same logical identity and payload. A gateway must not
deliver the same logical DATA packet to the application twice.

### Message types

Reserve the following values; implement only the current milestone’s
messages:

|   Type | Message        |
|-------:|----------------|
| `0x01` | HELLO          |
| `0x02` | ACK            |
| `0x10` | BEACON         |
| `0x11` | DATA           |
| `0x12` | ACK_BITMAP     |
| `0x20` | CONFIG_PROPOSE |
| `0x21` | CONFIG_ACCEPT  |
| `0x22` | CONFIG_COMMIT  |
| `0x30` | REJOIN_REQUEST |
| `0x31` | REJOIN_GRANT   |
| `0x7F` | DIAGNOSTIC     |

### First link: HELLO / ACK

- Client sends `HELLO` on the fixed initial data PHY at a configurable
  interval.
- Gateway validates the complete frame and network/destination fields.
- Gateway responds with `ACK` on the agreed fixed PHY.
- ACK payload must echo the original client source ID, boot ID, and
  sequence.
- Client validates the echoed identity; an ACK for another packet is not
  success.
- Client records success or timeout.
- Gateway handles duplicate HELLO frames idempotently.
- Log RSSI, SNR, SF, BW, frequency, sequence, and result.

Do not add TDMA/ADR to the first delivery merely because the final
architecture contains those features.

## 10. Future MAC design — implement by milestone

The accepted direction is:

**Gateway-Scheduled TDMA + Conservative ADR + ACK Bitmap + Selective
Retransmission.**

A single LLCC68 cannot receive multiple spreading factors
simultaneously. The gateway must switch its receiver to the expected PHY
for each scheduled client slot. It receives clients sequentially.

### Planned superframe

1.  Gateway broadcasts a beacon on the common control PHY.
2.  Clients validate network, frame ID, schedule version, and
    synchronization state.
3.  Each synchronized client transmits only in its assigned slot.
4.  Gateway switches RX configuration before the corresponding slot.
5.  Gateway validates packets and records per-client link metrics.
6.  Gateway returns an ACK bitmap on the common control PHY.
7.  Eligible clients use deterministic retry slots.
8.  The next superframe begins.
9.  Unsynchronized clients use a separate rejoin procedure; they do not
    transmit normal DATA slots.

Slot duration must account for:

`T_slot = T_air + T_radio_switch + T_processing + T_clock_guard`

Do not copy a guessed guard interval (including 25 ms) into production.
Measure clock drift, IRQ timing, and radio switching latency first.

### ACK bitmap and retry policy

- Bit `1`: the scheduled DATA packet was received and validated.
- Bit `0`: the expected packet was not received or failed validation.
- The bitmap’s bit-to-node mapping is defined by the active schedule
  version; never infer it from an unordered table.
- Bit order: least-significant bit first within each byte.
- 10 clients require 2 bytes; 70 clients require 9 bytes.
- Include `active_client_count`, `bitmap_length`, and
  `schedule_version`.
- Distinguish `MISSING` from `ACK_UNKNOWN` in client state.
- If the client receives a bitmap with its bit clear, it may retry in
  its deterministic retry slot.
- If the client receives no valid bitmap, its delivery state is
  `ACK_UNKNOWN`; it may retry in its assigned deterministic retry slot.
- Retry uses the same source ID, boot ID, sequence, and payload.
- Start with at most two retries; make the limit configurable.
- Gateway deduplicates repeated packets and can acknowledge a previously
  received packet again.
- After retry exhaustion, retain diagnostic evidence; never silently
  discard the failure.

### Conservative ADR

The gateway is the sole authority for data-PHY changes.

Initial ADR scope:

- fixed BW125 kHz;
- data SF7, SF8, or SF9 only;
- no dynamic bandwidth changes;
- no automatic TX-power changes in the first ADR release.

Use per-node RSSI, SNR, PDR/PER windows, ACK history, and retry count.
Thresholds must be centralized, documented, and labelled experimental
until field data supports them.

Rules:

- Increase robustness after repeated failures or sustained degradation.
- Decrease SF only after multiple stable windows with adequate margin.
- Hold the current profile when evidence is ambiguous.
- Use hysteresis; do not react to one packet.
- Never change a PHY in the middle of a superframe.
- Use `CONFIG_PROPOSE`, `CONFIG_ACCEPT`, and `CONFIG_COMMIT`.
- Carry `config_version` and `effective_frame_id`.
- If a commit is lost or not accepted, the client retains the previous
  committed profile.
- Keep the control PHY common and receivable by all enrolled clients.

## 11. Logging and diagnostics

Use parseable serial logs. One line per event; avoid long prose in the
hot path.

Recommended fields:

`uptime_us,role,node_id,event,frame_id,sequence,boot_id,config_version,schedule_version,sf,bw_khz,freq_mhz,rssi_dbm,snr_db,toa_us,retry_count,state,result`

At minimum log:

- boot and loaded configuration;
- provisioning and config validation errors;
- radio initialization and detected chip;
- TX start / TX done;
- RX done / RX timeout / CRC or decode failure where available;
- HELLO, ACK, duplicate, and sequence mismatch;
- beacon, slot start/end, ACK bitmap, retry;
- ADR proposal/accept/commit;
- synchronization loss and rejoin;
- radio driver error codes and recovery attempts.

Do not claim a PHY CRC failure count if the driver does not expose it.
Log `N/A` rather than inventing a value.

## 12. Build-iteration memory — mandatory

Every coding/build iteration must leave a durable Markdown record,
whether the build passes or fails.

### Required update sequence after each iteration

1.  Update `docs/en/BUILD_MEMORY.md` with a new append-only iteration
    entry.
2.  Update `docs/en/PROGRESS.md` as the current state snapshot.
3.  If an error, warning that blocks acceptance, failed test, or
    hardware issue occurred, add/update an entry in
    `docs/en/ERROR_LOG.md`.
4.  If an architectural or protocol decision changed, append an ADR
    entry to `docs/en/DECISIONS.md`.
5.  If externally visible firmware behavior or a release item changed,
    update `CHANGELOG.md`.
6.  Update the corresponding Chinese document when a technical decision
    or operator procedure changes.
7.  Save complete command output to `logs/build/` or `logs/test/` when
    output is material; reference the exact path from Markdown.

### Build-memory entry must record

- iteration ID, local date/time, author/agent, objective, milestone;
- firmware version, protocol version, Git branch and commit (or
  `not available`);
- exact files changed and concise responsibility of each change;
- exact commands executed, exit code, and pass/fail result;
- gateway/client role configuration used, board/module revision, and
  toolchain versions;
- unit-test names/counts/results;
- physical test setup: distance, LOS/NLOS, antenna, orientation, supply
  voltage, TX power, PHY, packet count;
- observed PDR/ACK success, RSSI/SNR, retries, latency, resets, and log
  references where measured;
- errors found, issue IDs, root cause if known, fix if attempted, and
  whether the fix was verified;
- known gaps, unverified assumptions, and exact next action.

Do not fill unknown fields with guesses. Write `NOT MEASURED`,
`NOT AVAILABLE`, or `TBD` explicitly.

### Append-only policy

- Never overwrite or delete an earlier iteration entry.
- Explicitly authorized publication identity/path redaction must preserve
  chronology, timestamps, outcomes and resolutions, disclose the operation
  in a new iteration, and retain private originals outside publication.
  Do not reinterpret old evidence as new validation.
- A correction must be appended as a correction note referencing the
  original iteration.
- Failed builds are still iterations and must be recorded.
- A successful compile is not a successful RF test.
- A host simulation is not a physical multi-node test.
- Do not mark a milestone `PASS` without acceptance evidence.
- Do not state that a command was run unless it was actually executed.

## 13. Milestones and acceptance gates

The intended bench pair is not assembled: two LLCC68 modules and one
MCU are available, the second MCU and Moxon antenna are pending. No
physical radio link is validated. Future two-device tests will not prove
multi-node collision avoidance or large-network performance.

| ID  | Milestone                      | Main acceptance evidence                                                                             |
|-----|--------------------------------|------------------------------------------------------------------------------------------------------|
| M0  | Hardware/toolchain baseline    | Board/module revisions, pin continuity, supply, antenna, legal RF check, resolved toolchain recorded |
| M1  | Radio bring-up                 | Both roles initialize LLCC68; BUSY/DIO1 behavior and driver status verified                          |
| M2  | Fixed-PHY HELLO/ACK            | Correct echo identity, timeout handling, duplicate handling, codec tests, two-device serial logs     |
| M3  | Beacon and one-client TDMA     | Frame ID, schedule version, slot timing, measured switching and guard time                           |
| M4  | ACK bitmap and selective retry | Host tests for 1/10/70 bits, loss injection, deterministic retry, deduplication                      |
| M5  | Versioned ADR                  | SF7/SF8/SF9 transitions, hysteresis, lost proposal/accept/commit tests                               |
| M6  | Synchronization recovery       | Missed beacon, reboot, stale schedule rejection, rejoin                                              |
| M7  | Moxon evaluation               | Controlled baseline-vs-Moxon comparison with identical PHY and placement                             |
| M8  | Multi-node preparation         | Host simulation for 5/10/20/35/70 logical nodes, clearly labelled simulation                         |

Longer-term planning is 70 clients across 7 gateways, initially around
10 clients per gateway, subject to RF survey and measured capacity. A
70-client single-gateway experiment is a separate scalability study. Do
not represent either target as already validated.

## 14. Coding rules

- Use readable, explicit C++.
- Prefer fixed-size buffers and bounded queues.
- Avoid heap allocation in radio, scheduler, and packet-processing hot
  paths.
- Avoid Arduino `String` in protocol and timing-critical modules.
- Never transmit raw packed structs.
- Avoid blocking `delay()` in timing-critical state machines.
- Use wrap-safe time comparisons.
- Check every RadioLib return code and every payload length.
- Keep pins and PHY profiles centralized.
- Keep the codec independent of Arduino and RadioLib.
- Keep state transitions explicit and testable.
- Comments must explain constraints or non-obvious reasoning, not
  restate the code.
- Do not refactor unrelated code.
- Do not introduce unsupported marketing language or performance claims.
- Do not create commits unless the user explicitly requests them.

## 15. Definition of done for a code iteration

Before reporting an iteration complete:

1.  Build the firmware environment.
2.  Run the relevant native unit tests.
3.  Inspect the complete diff for unrelated or accidental changes.
4.  Check that `main.cpp` remains a thin entry point.
5.  Update `BUILD_MEMORY.md` and `PROGRESS.md`.
6.  Update `ERROR_LOG.md` for failures and unresolved issues.
7.  Update `DECISIONS.md` if a decision changed.
8.  Update Chinese documentation when required.
9.  Report exact commands and actual results.
10. Separate compile evidence, host-test evidence, and physical RF
    evidence.

Final report must list:

- files changed and their responsibilities;
- firmware build result;
- native unit-test result;
- physical tests actually performed;
- unresolved errors and hardware assumptions;
- current milestone status;
- next concrete task.

Never claim an unexecuted test passed.

## 16. Independent architecture and publication

Maintain four target boundaries: protocol core, radio adapter, platform
services and application integration. Record implemented capabilities and
gaps from source; do not claim all four are complete. Protocol logic must
not depend on meter models, field protocols, backhaul, databases or UI.

RadioTransport and the LLCC68 adapter exist. A general application
payload/event API and complete clock/random/logging platform interfaces
do not yet exist. Future SX1262 support requires a separate adapter and
verification; do not change the current LLCC68 target during identity work.

Publish only sanitized project files and engineering records. Ignore SDK
caches, binaries, local configuration and credentials. Preserve dependency
license references and do not redistribute research PDFs without permission.
Use repository-relative paths or disclosed placeholders in historical logs.

## 17. Canonical development baseline

After the validated Iteration 013 integration, use `main` as the canonical development baseline. Start future milestone branches from an up-to-date `main`; preserve historical iteration branches and engineering records. Integrate reviewed work through pull requests using normal merge commits. Do not rewrite history, force-push, squash, rebase or delete historical branches as part of synchronization. A merged software baseline does not close physical acceptance gates.
