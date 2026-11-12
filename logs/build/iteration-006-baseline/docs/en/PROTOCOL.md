# Adaptive-LoRa Protocol

## 1. Protocol scope

The first protocol revision is a small, bounded binary protocol for a
two-device HELLO/ACK link. Later milestones add beacon scheduling,
bitmap ACK, retries, and ADR.

This is a custom LoRa P2P protocol. It is not LoRaWAN.

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
| `0x20` | CONFIG_PROPOSE | M5          |
| `0x21` | CONFIG_ACCEPT  | M5          |
| `0x22` | CONFIG_COMMIT  | M5          |
| `0x30` | REJOIN_REQUEST | M6          |
| `0x31` | REJOIN_GRANT   | M6          |
| `0x7F` | DIAGNOSTIC     | As required |

Do not implement reserved message types before their milestone.

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

M2 implements accepted ACK status 0 only, exactly nine payload bytes. Reject nonzero status, zero echoed boot ID, invalid echoed source, or echoed source different from the ACK destination. All M2 traffic is unicast: source/destination must be 1–65534 and different; network may be 1–65535. Reserved future message types and nonzero frame/config/schedule fields are rejected until their milestones. This clarifies M2 validation without changing the v1 layout.

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

M2 retains eight session keys, each with a 64-sequence replay window. Seen HELLOs are re-ACKed without a second accepted delivery; older sequences in a retained session are rejected as stale. Oldest-session eviction and gateway reboot remove history. This bounded two-unit cache does not implement persistent/authenticated replay protection or later DATA delivery guarantees. Re-evaluate capacity/lifetime at M4.

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

## 8. M3 beacon and schedule fields

M3 introduces a common control-PHY beacon. At minimum, it must carry:

- frame ID;
- schedule version;
- active client count;
- the client’s assigned slot information for the one-client test;
- timing information needed to schedule the uplink relative to beacon
  reception;
- validity/lease information so a stale client stops normal uplink.

For the one-client test, a fixed slot may be used. Do not assume that a
one-client schedule format is automatically sufficient for 70 clients.
Document and test any later schedule-table encoding before scaling.

The client measures local monotonic time at beacon RX completion and
schedules its assigned slot using the beacon’s relative timing fields.
The gateway uses its own TX completion and local monotonic clock. Guard
time must be based on measured radio switching and clock drift, not an
arbitrary constant.

## 9. M4 ACK_BITMAP

The ACK bitmap header identifies the superframe using `frame_id` and
carries the active `schedule_version`.

Payload structure:

| Field                 |     Size | Description                                |
|-----------------------|---------:|--------------------------------------------|
| `active_client_count` |        1 | Number of scheduled clients                |
| `bitmap_length`       |        1 | Must equal `ceil(active_client_count / 8)` |
| `bitmap`              | variable | One bit per client in schedule order       |

Bit order is least-significant bit first within each byte. Bit index `i`
maps to schedule entry `i` for the matching schedule version. The
schedule mapping must be stable and explicit.

Examples:

- 1 client → 1 byte.
- 10 clients → 2 bytes.
- 70 clients → 9 bytes.

Unused high bits in the final byte must be zero.

Client states must distinguish:

- `ACK_RECEIVED`: valid bitmap received and this client’s bit is one.
- `MISSING`: valid bitmap received and this client’s bit is zero.
- `ACK_UNKNOWN`: no valid bitmap received before the ACK window expires.

`MISSING` and `ACK_UNKNOWN` are not interchangeable.

## 10. Selective retransmission

- Retry slots are deterministic and tied to the active schedule.
- A client retries only in its assigned retry slot.
- A `MISSING` client may retry.
- An `ACK_UNKNOWN` client may retry in its assigned retry slot because
  the original packet may have reached the gateway while the bitmap was
  lost.
- Retry reuses the original logical packet identity and payload.
- Gateway deduplication prevents duplicate application delivery.
- Gateway may acknowledge a duplicate packet again.
- Initial maximum: two retries, configurable.
- After retry exhaustion, record the failure and sequence identity.

## 11. Conservative ADR

ADR is gateway-controlled and introduced only in M5.

Initial data profiles:

- SF7/BW125;
- SF8/BW125;
- SF9/BW125.

Keep the control PHY fixed at a common profile. Do not adapt bandwidth
or TX power in the first ADR release.

The gateway evaluates rolling per-client evidence: RSSI, SNR, PDR/PER,
ACK outcomes, and retries. Thresholds must be centralized and marked
experimental until measured.

Use the versioned handshake:

1.  Gateway sends `CONFIG_PROPOSE` with proposed profile, new
    `config_version`, and `effective_frame_id`.
2.  Client validates and returns `CONFIG_ACCEPT`.
3.  Gateway sends `CONFIG_COMMIT`.
4.  Both sides apply the profile only at the agreed frame boundary.
5.  If the client does not receive a valid commit, it keeps the previous
    committed profile.

Never switch PHY in the middle of a superframe.

## 12. Protocol versioning

`PROTOCOL_VERSION` describes the wire format. It is independent of
firmware SemVer, `config_version`, and `schedule_version`.

Bump the protocol version for incompatible changes to header layout,
message encoding, or interpretation. Document compatibility behavior
before supporting multiple versions.
