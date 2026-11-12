# Architecture

## 1. Design overview

The project uses one firmware image for two physically identical
ESP32-C3 + Ra-01SCH devices.

``` text
                         One firmware image
                                |
                             Boot
                                |
                       Load config from NVS
                                |
                    Validate schema and device IDs
                                |
                 +--------------+--------------+
                 |              |              |
              GATEWAY         CLIENT        Invalid / unset
                 |              |              |
            GatewayApp       ClientApp    Provisioning mode
                 |              |          (TX disabled)
                 +--------------+--------------+
                                |
                         RadioDriver
                                |
                       Llcc68Hardware
                                |
                       RadioLib LLCC68
                                |
                         Ra-01SCH module
```

Role selection is configuration-driven, not hardware-detected. The
boards are identical, so the firmware cannot infer which physical unit
is intended to be the gateway. Keep both roles and shared modules in one
repository and one Codex workspace where possible; use milestone-scoped
tasks rather than separate, drifting codebases.

## 2. Boot and runtime flow

1.  Initialize serial diagnostics.
2.  Initialize the NVS configuration store.
3.  Read the configuration schema version and device settings.
4.  Validate role, network ID, node ID, gateway ID, and reserved values.
5.  If configuration is invalid, enter provisioning mode. Do not start
    normal radio transmission.
6.  If valid, construct exactly one role application.
7.  Initialize the radio through the single `RadioDriver` owner.
8.  Call the selected application’s non-blocking `begin()` and
    `update()` methods from the runtime loop.

A configuration schema mismatch must not be interpreted as a valid role.
Provide a clear diagnostic and require re-provisioning or a deliberate
migration.

## 3. Serial provisioning contract

Serial speed: 115200 baud.

Minimum commands:

| Command                           | Purpose                                           |
|-----------------------------------|---------------------------------------------------|
| `help`                            | Show supported commands                           |
| `status`                          | Show runtime mode, role, IDs, schema, radio state |
| `config show`                     | Show current configuration                        |
| `config set role gateway\|client` | Select device role                                |
| `config set network-id N`         | Set network ID                                    |
| `config set node-id N`            | Set this device ID                                |
| `config set gateway-id N`         | Set gateway ID                                    |
| `config save`                     | Validate and persist configuration                |
| `reboot`                          | Restart after configuration                       |

Initial bench configuration:

| Parameter  | Gateway | Client |
|------------|--------:|-------:|
| Role       | GATEWAY | CLIENT |
| Network ID |       1 |      1 |
| Node ID    |       1 |      2 |
| Gateway ID |       1 |      1 |

Node/gateway IDs 0 and 65535 are reserved. For the gateway, `node_id` equals
`gateway_id`. For a client, `node_id` must differ from `gateway_id`.

The command parser must be bounded, reject malformed numeric values, and
return a clear error without corrupting the last valid configuration.

Implementation details (ADR-009): 128-byte line buffer, six-token maximum, 64-byte service budget per loop; overflow/control-byte lines are discarded through newline. Add `config set hello-interval-ms <1000..3600000>`; default 5000 ms after a transaction completes. Values are staged, never applied to the loaded app until reboot. Role edits preserve IDs. Network 65535 is allowed; the 65535 broadcast reservation applies to node addresses.

Schema 1 persists a single 24-byte explicit blob with corruption-detection hash and verifies readback. Invalid load keeps UNSET and leaves the radio uninitialized. `status` labels unavailable loaded values as `inactive_defaults`; `config show` reports staged values. Controlled reboot stops new requests, finishes or expires active TX, requests standby and restarts. Initial radio failure requires inspection/reboot; RX-start recovery is limited to three consecutive failures, spaced one second apart.

NVS namespace/key: `legacy schema-1 namespace` / `device`. Storage integers are little-endian (not a wire-format extension):

The original namespace bytes are retained as a numeric compatibility constant in NvsConfigStore so existing saved configuration remains addressable. This is a storage identifier, not a project identity, application dependency or protocol field. Its decoded historical label is not publication content. Schema, keys, blob and on-air bytes are unchanged by repository establishment.

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | magic 0x4344464D |
| 4 | 2 | schema 1 |
| 6 | 1 | role: 1 gateway / 2 client; 0 is unset and never saved |
| 7 | 1 | reserved 0 |
| 8 | 2 | network ID |
| 10 | 2 | node ID |
| 12 | 2 | gateway ID |
| 14 | 2 | reserved 0 |
| 16 | 4 | HELLO interval in milliseconds |
| 20 | 4 | FNV-1a over bytes 0–19, initial 2166136261, xor byte then multiply 16777619 modulo 2^32 |

## 4. Module responsibilities

| Module                | Responsibility                                          | Must not do                                        |
|-----------------------|---------------------------------------------------------|----------------------------------------------------|
| `main.cpp`            | Arduino entry point; delegate to runtime                | Radio calls, protocol logic, app state machine     |
| `FirmwareRuntime`     | Load config, select role, dispatch loop                 | Own a second radio                                 |
| `DeviceConfig`        | Pure configuration model and validation                 | Access NVS or radio hardware                       |
| `NvsConfigStore`      | Persist validated configuration through Preferences/NVS | Validate protocol frames or store packet telemetry |
| `ProvisioningConsole` | Parse serial commands and report config                 | Run radio TX/RX                                    |
| `GatewayApp`          | Gateway state machine and HELLO validation/ACK response | Direct SPI operations                              |
| `ClientApp`           | Client state machine, HELLO/ACK tracking                | Direct SPI operations                              |
| `RadioDriver`         | TX/RX state, deadlines, validation and event policy     | Direct RadioLib/SPI/GPIO access                    |
| `Llcc68Hardware`      | Sole Module/LLCC68/SPI/GPIO/ISR adapter                 | Own driver or application state transitions        |
| `FrameCodec`          | Serialize/deserialize bounded frames                    | Depend on Arduino or RadioLib                      |
| `SequenceTracker`     | Track duplicate packet identities                       | Perform radio operations                           |
| `SerialLogger`        | Format structured diagnostics                           | Block timing-critical radio work                   |

Keep the architecture small. A module should exist because it owns a
concrete responsibility, not because every function needs a class.

## 5. Radio ownership

Only `Llcc68Hardware` within the radio module may access RadioLib, SPI or
the radio GPIOs. `RadioDriver` owns state policy through a non-owning
`RadioHardware` reference (ADR-012); Runtime constructs one adapter and driver.

The first release should use one cooperative event-driven loop. The
radio driver reports events such as `TX_DONE`, `RX_DONE`, `RX_TIMEOUT`,
and `RADIO_ERROR`. Applications consume those events and advance state.

DIO/IRQ callbacks must do minimal work only. They may set a flag or
enqueue a minimal event. They must not call SPI, parse frames, allocate
memory, or print logs.

## 6. Initial M2 data flow

``` text
ClientApp                 RadioDriver                 GatewayApp
   |                           |                           |
   |-- request HELLO TX ------>|                           |
   |                           |==== HELLO over LoRa =====>|
   |                           |<--- RX_DONE + metrics -----|
   |                           |                           |-- decode/validate
   |                           |                           |-- duplicate check
   |                           |<====== request ACK TX =====|
   |<========= ACK over LoRa ==|                           |
   |-- validate echoed ID -----|                           |
   |-- success or timeout      |                           |
```

The ACK must echo the original client source ID, boot ID, and sequence.
The gateway’s own boot ID in the ACK header is not a substitute for the
echoed identity.

## 7. Later MAC flow

The accepted direction is gateway-scheduled TDMA with conservative
per-client SF adaptation, bitmap ACK, and selective retry.

The gateway has one LLCC68. It cannot listen to multiple spreading
factors at once. For each scheduled client slot, it configures the
receiver for the expected PHY, receives that client sequentially, then
changes back to the common control PHY for the ACK bitmap.

The superframe is planned as:

1.  Common-PHY beacon.
2.  Scheduled sequential client uplinks.
3.  Common-PHY ACK bitmap.
4.  Deterministic retry slots.
5.  Rejoin/control window as required.

Control and data are logical planes that time-share the same
transceiver.

## Software verification boundary (ADR-010)

GatewayApp and ClientApp depend on the six-method RadioTransport reference; RadioDriver implements it and delegates LLCC68/SPI/IRQ operations to the sole Llcc68Hardware adapter. Runtime still owns the concrete driver and calls begin/poll/stop. A native fake supplies completed packet/error events to the actual production apps; test-only clock/serial headers let the actual logger run. No state-machine implementation is duplicated in tests, and no runtime heap allocation is introduced by the seam.

The app logic is cooperative, but NVS commits, RadioLib BUSY waits and SDK serial calls are synchronous. A bounded logging queue is not a hard nonblocking guarantee for those dependencies. RadioLib also uses heap scratch buffers by default. Host app tests cannot validate hardware latency, ISR races, flash or RF.

M2 still has no automatic retransmissions: tests replay the captured bytes unchanged to exercise identity/deduplication. New transactions after timeout use a new sequence. Inventory is two modules/one MCU/no fabricated Moxon, so physical bring-up is deferred.

## 8. Scaling boundary

Current physical evidence can only come from one gateway and one client.
Later host simulations may model 5, 10, 20, 35, or 70 logical clients,
but simulation results must be labelled as simulation.

Longer-term planning is 70 clients across 7 gateways, initially around
10 clients per gateway. This is a planning assumption pending RF survey,
airtime calculation, and physical tests. Do not claim 70-node field
capacity from a two-device bench link.

## 9. Independent integration architecture: implementation versus target

Adaptive-LoRa targets reusable embedded-network logic. The research title describes future scheduling/adaptation work, not capabilities already delivered by firmware 0.2.0-alpha.1.

| Boundary | Verified current implementation | Target and remaining gap |
|---|---|---|
| Protocol core | Bounded LE FrameCodec, packet identity, sequence/session helpers and M2 gateway/client HELLO/ACK states | No TDMA/superframe, bitmap, selective retry, ADR or general DATA service. Apps still directly call the ESP timer and concrete SerialLogger. Core helper modules are platform-independent; the entire role runtime is not. |
| Radio abstraction | Six-method RadioTransport reference, bounded RadioEvent and one RadioDriver state owner with a referenced Llcc68Hardware adapter | Initialization/poll/stop remain concrete driver entry points. RadioHardware now abstracts existing fixed-profile hardware calls for fault injection, not a general adapter configuration service; no SX1262 adapter exists. |
| Platform services | ESP32/Arduino FirmwareRuntime, NvsConfigStore, bounded console, pins, IRQ flag, hardware random and timer calls | No general clock/random/persistence/interrupt service contract. Native compatibility headers are test adapters, not production ports to other MCUs. |
| Application integration | HELLO carries no application payload in the current client; codec can bound opaque payload bytes; diagnostics expose link events/counters as serial logs | No submission API, received-payload callback, link-event subscriber or reusable application statistics contract. Add these in a separate reviewed task before integrating another application. |

Modbus, meter models, Wi-Fi/MQTT backhaul, databases and dashboards are outside the protocol core and absent from source. Future integrations must use the application boundary rather than introduce such dependencies into codec/state/scheduling logic. Industrial energy monitoring remains a general motivation, not a required deployment.

Keep future protocol, radio, platform and application changes milestone-scoped. Iteration 008 renamed internal identifiers and documented gaps; iteration 009 adds only the driver verification seam, not the missing integration APIs or later MAC features. Synchronous SDK waits/allocation and physical validation limits remain in SOFTWARE_AUDIT.md and ERROR_LOG.md.

## 10. Radio-driver verification boundary (ADR-012)

RadioDriver has no Arduino/RadioLib dependency. Its begin/receive/transmit/stop/poll logic runs in native tests through RadioHardware. Llcc68Hardware owns the single Module/LLCC68, fixed PHY/SPI setup, IRQ callback and GPIO sampling. The callback only sets a volatile flag; sampling then clearing still depends on latched high DIO1 as before. The six-method application transport is unchanged.

The bounded hardware fake supplies operation results, IRQ latch/level/flags and packet bytes. This tests production driver policy, not the adapter, RadioLib implementation, chip commands, latency or physical ISR races. Numeric status/raw IRQ values are compile-checked against RadioLib 7.8.1; fixed arguments/call ordering are reviewed separately. All previous platform/application API and dependency timing/allocation gaps remain.
