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
| `RadioDriver`         | Own LLCC68 instance, SPI, TX/RX, IRQ events             | Decide application protocol policy                 |
| `FrameCodec`          | Serialize/deserialize bounded frames                    | Depend on Arduino or RadioLib                      |
| `SequenceTracker`     | Track duplicate packet identities                       | Perform radio operations                           |
| `SerialLogger`        | Format structured diagnostics                           | Block timing-critical radio work                   |

Keep the architecture small. A module should exist because it owns a
concrete responsibility, not because every function needs a class.

## 5. Radio ownership

Only `RadioDriver` may access RadioLib or the SPI peripheral used by the
LoRa module.

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

GatewayApp and ClientApp depend on the six-method RadioTransport reference; RadioDriver implements it and alone owns all LLCC68/SPI/IRQ operations. Runtime still owns the concrete driver and calls begin/poll/stop. A native fake supplies completed packet/error events to the actual production apps; test-only clock/serial headers let the actual logger run. No state-machine implementation is duplicated in tests, and no runtime heap allocation is introduced by the seam.

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
