# Architecture

The bench pair is Heltec WiFi LoRa 32 V3.4. The owner confirms V3.2
wiring, so the build uses the Arduino V3 variant and V3.2 electrical reference.
The default research image uses protocol v3 and NVS schema 1. See
[HARDWARE.md](HARDWARE.md) for evidence by revision.

## 1. Design overview

Every board runs the same image. `FirmwareRuntime` loads and validates role
and IDs from NVS, then starts the corresponding `MulticlientLink` state
machine. Invalid or missing configuration leaves application TX disabled.
The v1 target still runs `GatewayApp` or `ClientApp`, and the v2 target uses
`ScheduledLink`. Use the same protocol version on every board in a test.

`MulticlientLink` uses `PayloadPort` for application delivery and reaches the
SX1262 through the sole radio owner, `RadioDriver`. The [protocol flow in the
README](../../README.md#how-it-works) shows how Gateway grants, DATA slots
and feedback fit together. [Scheduled research links](SCHEDULED_LINK.md)
specifies the wire format and timing rules.

## 2. Boot and runtime flow

1.  Initialize serial diagnostics.
2.  Initialize the NVS configuration store.
3.  Read the configuration schema version and device settings.
4.  Validate role, network ID, node ID, gateway ID, and reserved values.
5.  If configuration is invalid, enter provisioning mode. Do not start
    normal radio transmission.
6.  If valid, initialize the radio through the single `RadioDriver` owner.
7.  Only after successful initialization, construct and begin the selected role application.
8.  Poll the driver, advance the application at a fresh timestamp, service bounded
    serial input and drain diagnostics. Dependency calls can wait synchronously;
    cooperative state machines do not establish a hard loop-time bound.

In the display build, Esp32Firmware owns the display before the runtime. The
runtime starts display power/reset sequencing even for invalid configuration.
It takes snapshots after primary loop work and skips display service during TX,
client ACK wait and reboot. Application snapshots provide counters and the
last qualifying reception; display never changes radio, application or NVS state.
A failed display operation produces a display fault event. In the v3 build,
the display can retry after a delay. The runtime uses `MulticlientLink`
telemetry, checks for a quiet tail while scheduled, and polls RX between
display steps while the Client searches for a beacon. See
[display diagnostics](DISPLAY_DIAGNOSTICS.md) for the runtime guard and fault policy.

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
| `config show`                     | Show staged configuration                        |
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

Implementation details (ADR-009): 128-byte line buffer, six-token maximum, 64-byte service budget per loop; overflow/control-byte lines are discarded through newline. Add `config set hello-interval-ms <1000..3600000>`; default 3,000 ms after a transaction completes. Values are staged, never applied to the loaded app until reboot. Role edits preserve IDs. Network 65535 is allowed; the 65535 broadcast reservation applies to node addresses.

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
| `FirmwareRuntime`     | Load config, select role, dispatch loop via existing injected storage/radio boundaries | Duplicate role/state policy |
| `Esp32Firmware`       | Own concrete ESP32 adapters before the single coordinator | Contain protocol or runtime policy |
| `DeviceConfig`        | Pure configuration model and validation                 | Access NVS or radio hardware                       |
| `NvsConfigStore`      | Validate/encode/decode config, classify storage errors and verify readback | Own Preferences or store packet telemetry |
| `PreferencesConfigStorage` | Sole Preferences owner; five direct session/byte operations | Validate, serialize or verify configurations |
| `ProvisioningConsole` | Parse serial commands and report config                 | Run radio TX/RX                                    |
| `GatewayApp`          | Gateway state machine and HELLO validation/ACK response | Direct SPI operations                              |
| `ClientApp`           | Client state machine, HELLO/ACK tracking                | Direct SPI operations                              |
| `ScheduledLink`       | Opt-in v2 beacon, slot timing, DATA and bitmap state     | Direct SPI operations                              |
| `RadioDriver`         | TX/RX state, deadlines, validation and event policy     | Direct RadioLib/SPI/GPIO access                    |
| `Sx1262Hardware`      | Sole Module/SX1262/SPI/GPIO/ISR adapter                 | Own driver or application state transitions        |
| `FrameCodec`          | Serialize/deserialize bounded frames                    | Depend on Arduino or RadioLib                      |
| `SequenceTracker`     | Track duplicate packet identities                       | Perform radio operations                           |
| `SerialLogger`        | Format structured diagnostics                           | Block timing-critical radio work                   |

Keep the architecture small. A module should exist because it owns a
concrete responsibility, not because every function needs a class.

## 5. Radio ownership

Only `Sx1262Hardware` within the radio module may access RadioLib, SPI or
the radio GPIOs. `RadioDriver` owns state policy through a non-owning
`RadioHardware` reference (ADR-012); Esp32Firmware constructs the sole concrete adapter before the reference-injected runtime/driver.

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

## 7. Scheduled MAC flow

The default v3 image uses beacon, length request, exact grant, DATA and
aggregate feedback phases with up to three assigned retries. `PayloadPort`
copies client bytes and invokes a bounded gateway receiver. The v2 image
continues to provide its single-client protocol. `FirmwareRuntime` supplies the maintained monotonic
clock reading after polling `RadioDriver`. It selects the v2 role from the
same validated NVS configuration used by the M2 image. See the
[scheduled calendar](SCHEDULED_LINK.md) for phase offsets and bench limitations.

All active phases use SF7. Future ADR would add a gateway-controlled profile
assignment and a guarded retune between slots, plus an explicit rejoin
window. The gateway owns one SX1262, so these phases are time-multiplexed.

## Current platform and integration boundaries

The V3 board environment selects ESP32-S3, CP2102/UART0 at115200 and
CDC_ON_BOOT=0. The sole radio adapter uses GPIO8-14, DIO3 TCXO1.8 V,
DIO2 RF switching and DC-DC. Battery sensing, Wi-Fi and backhaul are
outside the firmware. The driver retains header/CRC reassertions and all
existing deadlines/error priority. Its dependency calls can still block;
ERR-021 is not fixed by selecting SX1262.

Codec/config/identity helpers remain platform-independent. Applications
still call ESP timers and SerialLogger, so the full runtime is not a
generic MCU port. The v3 payload boundary provides bounded copy-in and receive callbacks. A general application event API remains open.
The radio/storage reference boundaries support focused native tests,
not an unimplemented cross-platform integration framework.

## Historical architecture and verification notes through Iteration013

The following notes retain the source context of earlier iterations.
Hardware names, inventories and “not yet implemented” statements describe
those snapshots; the current hardware decision above supersedes them.

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
| Protocol core | Bounded LE FrameCodec, packet identity, sequence/session helpers and M2 gateway/client HELLO/ACK states | The v3 `MulticlientLink` provides up to eight scheduled clients and invokes the application receiver through `PayloadPort`. ADR and persistent delivery remain open. Apps still directly call the ESP timer and concrete SerialLogger. Core helper modules are platform-independent; the entire role runtime is not. |
| Radio abstraction | Six-method RadioTransport reference, bounded RadioEvent and one RadioDriver state owner with an SX1262 adapter | Initialization, poll and stop remain concrete driver entry points. RadioHardware abstracts fixed-profile hardware calls for fault tests. Runtime retune and alternative adapters remain future work. |
| Platform services | ESP32/Arduino FirmwareRuntime, NvsConfigStore, bounded console, pins, IRQ flag, hardware random and timer calls | A narrow ConfigStorage session/byte seam now supports store fault tests. No general clock/random/interrupt service or production port to other MCUs is implemented. |
| Application integration | HELLO carries no application payload in the current client; codec can bound opaque payload bytes; diagnostics expose link events/counters as serial logs | No submission API, received-payload callback, link-event subscriber or reusable application statistics contract. Add these in a separate reviewed task before integrating another application. |

Modbus, meter models, Wi-Fi/MQTT backhaul, databases and dashboards are outside the protocol core and absent from source. Future integrations must use the application boundary rather than introduce such dependencies into codec/state/scheduling logic. Industrial energy monitoring remains a general motivation, not a required deployment.

Keep future protocol, radio, platform and application changes milestone-scoped. Iteration 008 renamed internal identifiers and documented gaps; iteration 009 adds only the driver verification seam, not the missing integration APIs or later MAC features. Synchronous SDK waits/allocation and physical validation limits remain in SOFTWARE_AUDIT.md and ERROR_LOG.md.

## 10. Radio-driver verification boundary (ADR-012)

RadioDriver has no Arduino/RadioLib dependency. Its begin/receive/transmit/stop/poll logic runs in native tests through RadioHardware. Llcc68Hardware owns the single Module/LLCC68, fixed PHY/SPI setup, IRQ callback and GPIO sampling. The callback only sets a volatile flag; sampling then clearing still depends on latched high DIO1 as before. The six-method application transport is unchanged.

The bounded hardware fake supplies operation results, IRQ latch/level/flags and packet bytes. This tests production driver policy, not the adapter, RadioLib implementation, chip commands, latency or physical ISR races. Numeric status/raw IRQ values are compile-checked against RadioLib 7.8.1; fixed arguments/call ordering are reviewed separately. All previous platform/application API and dependency timing/allocation gaps remain.

## 11. Configuration-store verification boundary (ADR-013)

NvsConfigStore depends on five-method ConfigStorage by reference. PreferencesConfigStorage owns the only Preferences object and forwards namespace/key, bool/count results and explicit end without config policy. Esp32Firmware constructs this adapter before the reference-injected runtime/store; no heap ownership or generic persistence framework. The original numeric namespace stays in the store, device key/schema/24-byte blob remain compatible, and DeviceConfig/console/runtime implementation is unchanged.

Load opens read-only, checks length, reads at most 24 bytes, closes, then decodes. Save validates/encodes before access, opens writable, writes/closes, then invokes actual load in a new read-only session and compares exact encoded bytes. All successful opens have explicit closes; failed begin requires no end. Current serialized usage is preserved; this seam does not add concurrent/nested transaction support.

Native executes actual store/config/codec against bounded backend responses. It verifies classification, cleanup and readback policy, not the real adapter/NVS internals or flash. Failure does not imply rollback; successful host readback does not prove hardware commit/power-loss atomicity/durability. Runtime's invalid-config early return before radio.begin and staged-until-reboot policy are source-preserved, not newly executed by store tests.

## 12. Runtime composition and verification (ADR-014)

Esp32Firmware owns PreferencesConfigStorage, Llcc68Hardware and then FirmwareRuntime in that declaration order; destruction reverses it. Runtime owns actual NvsConfigStore and RadioDriver through the existing lower-level references, plus the existing console/logger/apps. One image, one coordinator, one radio and one storage owner. Main's only change is composition include/type; it remains15 lines.

Native compiles the real coordinator with the existing actual components. SDK header adapters supply bounded serial input, shared virtual time/micros, observed reset/reason and scripted raw random values; actual freshBootId rejection and app session behavior execute. There is no new radio/store interface or separate runtime model. Header availability selects standard optional on firmware, experimental optional on the legacy host; native enables full-width Unity assertions. These adapters do not constitute a general platform-service API.

Verified policy retains app-before-command dispatch, <=64 input bytes and <=128 logger drain per loop, invalid config early return before radio/app/entropy, radio before role activation, staged versus loaded config, TX-done/deadline handling and100ms reboot arming. Stop failure still logs and proceeds; reset does not wait for queue/USB drain. Both are existing observed policies, not fixed by this task. Hardware blocking/entropy/liveness and real physical lifecycle remain unverified.

## 13. Session failure and timing boundaries (ADR-015)

Iteration012 bounds actual raw generation at8 calls, with0 indicating failure. SequenceCounter renewal preserves prior boot/exhausted values on0/same-ID; apps never reset to a failed identity. Client HELLO and Gateway ACK TX are inhibited while unavailable; SESSION_ERROR and1s attempt-start retry maintain observable fail-closed recovery, disabled while reboot pending. Successful recovery uses fresh boot/sequence0; no NVS or wire-format change. Valid-value filtering is not entropy/security or cross-reboot uniqueness.

Runtime passes post-poll time to apps, arms reset100ms after stop returns and evaluates current time after logger drain, correcting ERR-020. This does not preempt blocking calls, timestamp RF completion or eliminate dependency latency. Direct driver/app inputs retain their explicit observation-time contract. See DEPENDENCY_BUDGET_AUDIT.md for exact source-based timing/allocation/ELF budgets, including upstream unbounded BUSY-ramp risk and physical exclusions.
