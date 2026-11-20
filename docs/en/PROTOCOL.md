# Adaptive-LoRa Protocol

## 1. Protocol scope

Adaptive-LoRa uses a custom LoRa P2P protocol. The default and OLED builds
use v1 HELLO/ACK. The single-client research build uses v2. The multi-client build uses v3.
Both are specified in [SCHEDULED_LINK.md](SCHEDULED_LINK.md). Each
decoder accepts its own version and types only. `PROTOCOL_VERSION` tracks
the default v1 wire format. Firmware version and NVS schema are shared.

The saved role selects Gateway or Client in each image. FirmwareRuntime
services one RadioDriver; Sx1262Hardware owns the peripheral and latches
DIO1. All profiles use provisional 915.0 MHz, SF7/BW125, CR4/5, preamble 8,
explicit header, CRC, sync 0x12 and requested 22 dBm. SF9 stays reserved.
M2 waits one second from serviced TX_DONE for a matching ACK. The driver
TX timeout is two seconds. M2 starts a new HELLO after the configured
reporting interval (3,000 ms by default) and never retries the original
HELLO. The next interval begins when the transaction finishes.

## 2. Frame limits and byte order

- Maximum complete application frame: 96 bytes.
- Fixed header length: 28 bytes.
- Maximum payload: 68 bytes.
- Multi-byte integers: unsigned little-endian.
- PHY CRC: enabled.
- No raw C++ struct transmission.
- No compiler-dependent packing.
- Decoder must reject truncation, excess bytes, unsupported versions,
  invalid lengths, and unknown message types.

## 3. Protocol v1 header

| Offset | Field              | Size | Encoding | Description                                    |
|-------:|--------------------|-----:|----------|------------------------------------------------|
|      0 | `magic`            |    2 | u16 LE   | Constant `0x4D4C`                              |
|      2 | `protocol_version` |    1 | u8       | `1`                                            |
|      3 | `message_type`     |    1 | u8       | Type table below                               |
|      4 | `flags`            |    1 | u8       | Reserved; transmit as zero in v1               |
|      5 | `header_length`    |    1 | u8       | Must equal `28`                                |
|      6 | `network_id`       |    2 | u16 LE   | Network identifier; zero reserved              |
|      8 | `source_id`        |    2 | u16 LE   | Sender ID                                      |
|     10 | `destination_id`   |    2 | u16 LE   | Receiver ID; `0xFFFF` reserved for broadcast   |
|     12 | `boot_id`          |    4 | u32 LE   | Sender session identifier, regenerated on boot |
|     16 | `frame_id`         |    4 | u32 LE   | Superframe ID; zero in M2 HELLO/ACK            |
|     20 | `sequence`         |    2 | u16 LE   | Sender sequence number                         |
|     22 | `config_version`   |    2 | u16 LE   | Committed PHY/config version; zero before ADR  |
|     24 | `schedule_version` |    2 | u16 LE   | Schedule version; zero before TDMA             |
|     26 | `payload_length`   |    1 | u8       | Exact payload byte count                       |
|     27 | `reserved`         |    1 | u8       | Must be zero in v1                             |
|     28 | `payload`          | 0–68 | bytes    | Message-specific data                          |

Total length must equal `28 + payload_length` and must not exceed 96
bytes.

The magic value is written little-endian. Its on-air bytes are
`0x4C 0x4D`.

## 4. Message type allocation

|  Value | Type           | Milestone   |
|-------:|----------------|-------------|
| `0x01` | HELLO          | M2          |
| `0x02` | ACK            | M2          |
| `0x10` | BEACON         | M3          |
| `0x11` | DATA           | M3+         |
| `0x12` | ACK_BITMAP     | M4          |
| `0x13` | PAYLOAD_REQUEST | v3          |
| `0x14` | SLOT_GRANT      | v3          |
| `0x20` | CONFIG_PROPOSE | M5          |
| `0x21` | CONFIG_ACCEPT  | M5          |
| `0x22` | CONFIG_COMMIT  | M5          |
| `0x30` | REJOIN_REQUEST | M6          |
| `0x31` | REJOIN_GRANT   | M6          |
| `0x7F` | DIAGNOSTIC     | As required |

HELLO and ACK are v1 only. BEACON, DATA and ACK_BITMAP exist in v2 and
v3 with distinct layouts. PAYLOAD_REQUEST and SLOT_GRANT are v3 only.
Future ADR and rejoin types remain reserved.

## 5. M2 HELLO / ACK payload

### HELLO

The first HELLO may use a zero-length payload. The header carries the
sender’s network ID, source ID, boot ID, sequence, and destination ID.

The M2 codec permits 0–68 opaque HELLO bytes to exercise the full bounded frame; the shipped client sends zero bytes. No meter payload interpretation is implemented.

### ACK

The ACK header identifies the gateway as the sender and the client as
the destination. Its payload echoes the original HELLO identity:

| Field             | Size | Encoding                |
|-------------------|-----:|-------------------------|
| `acked_source_id` |    2 | u16 LE                  |
| `acked_boot_id`   |    4 | u32 LE                  |
| `acked_sequence`  |    2 | u16 LE                  |
| `status`          |    1 | u8; zero means accepted |

The client reports success only if all echoed fields match the
outstanding HELLO. A valid ACK for another boot ID or sequence is not
success.

M2 implements accepted ACK status 0 only, exactly nine payload bytes. Reject nonzero status, zero echoed boot ID, invalid echoed source, or echoed source different from the ACK destination. All M2 traffic is unicast: source/destination must be 1–65534 and different; network may be 1–65535. Reserved future message types and nonzero frame/config/schedule fields are rejected by v1. This clarifies M2 validation without changing the v1 layout.

## 6. Boot ID and sequence behavior

- Generate a nonzero `boot_id` at startup using the ESP32 hardware
  random source.
- A boot ID is a session identifier, not an authentication credential.
- Sequence starts from a defined initial value after boot and increments
  for each new logical application packet.
- A retransmission reuses the same sequence and boot ID.
- Before the 16-bit sequence counter wraps, stop issuing new application
  packets, finish or expire outstanding transactions, generate a new
  nonzero `boot_id` session value, reset the sequence counter, and
  re-establish the session with HELLO/rejoin. Do not reuse a sequence
  within the same boot/session ID.
- The deduplication key is
  `network_id + source_id + boot_id + sequence`.

M2 retains eight session keys, each with a 64-sequence replay window. Seen HELLOs are re-ACKed without a second accepted delivery; older sequences in a retained session are rejected as stale. Round-robin replacement of an entry and gateway reboot remove history. This bounded two-unit cache does not implement persistent/authenticated replay protection or later DATA delivery guarantees. Re-evaluate capacity/lifetime at M4.

## 7. Validation rules

The decoder must validate, in this order or an equivalent safe order:

1.  Input pointer and actual frame length.
2.  Minimum header length.
3.  Magic.
4.  Protocol version.
5.  Header length.
6.  Reserved fields and flags.
7.  Message type.
8.  Payload length and exact total length.
9.  Network ID.
10. Destination ID.
11. Message-specific payload length and field ranges.

Do not read a field before confirming the input contains its bytes. Do
not copy a payload into a destination buffer before confirming capacity.

## 8–10. Scheduled research protocols (v2 and v3)

[SCHEDULED_LINK.md](SCHEDULED_LINK.md) defines byte layouts, identity and
validation, the payload-aware timing equations, feedback, and retries for
the two scheduled profiles. The older schedule-table candidates in
[TDMA_FEASIBILITY.md](TDMA_FEASIBILITY.md) are historical design inputs.

## 11. Planned conservative ADR

ADR is gateway-controlled and introduced only in M5.

Initial data profiles:

- SF7/BW125;
- SF8/BW125;
- SF9/BW125.

Keep the control PHY fixed at a common profile. Do not adapt bandwidth
or TX power in the first ADR release.

The gateway evaluates rolling per-client evidence: RSSI, SNR, PDR/PER,
ACK outcomes, and retries. Thresholds must be centralized and marked
experimental until measured. No production thresholds or hysteresis
windows have been selected. A confirmed binary or interpretation change
requires the version and coexistence review in §12.

Use the versioned handshake:

1.  Gateway sends `CONFIG_PROPOSE` with proposed profile, new
    `config_version`, and `effective_frame_id`.
2.  Client validates and returns `CONFIG_ACCEPT`.
3.  Gateway sends `CONFIG_COMMIT`.
4.  Both sides apply the profile only at the agreed frame boundary.
5.  If the client does not receive a valid commit, it keeps the previous
    committed profile.

Commit a different profile assignment only at an agreed superframe boundary.
Future scheduled retunes apply the already active profile at a declared
phase or slot boundary. The current driver has no such retune path.

## 12. Protocol versioning

`PROTOCOL_VERSION` describes the default M2 wire format. The scheduled
builds declare v2 in `scheduled_protocol.h` and v3 in `multiclient_protocol.h`. It is independent of
firmware SemVer, `config_version`, and `schedule_version`.

Bump the protocol version for incompatible changes to header layout,
message encoding, or interpretation. Document compatibility behavior
before supporting multiple versions.
