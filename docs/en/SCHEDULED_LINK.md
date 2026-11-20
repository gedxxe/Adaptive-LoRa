# Scheduled research links

## Multi-client protocol v3

Build `heltec_wifi_lora_32_V3_multiclient` on both roles. It uses SF7 for
every phase and reports `profile=multiclient, protocol=3` in `status`. The
[startup guide](STARTUP_AND_PROVISIONING.md) covers role selection and
gateway admission. Use the v2 environment described below for legacy tests.

### Wire format

The v3 [common envelope](PROTOCOL.md) is 28 bytes, with 96 bytes maximum
for the complete frame. Version byte 2 equals `3`. Flags at 4, config version
at 22–23 and reserved byte 27 are zero. Schedule version at 24–25 is `1`.
Boot ID and frame ID must be nonzero. Network, destination, type and exact
length are checked before the role interprets the body. All integers are
little-endian. Only the configured gateway can supply control to a client.

| Type | Destination and header sequence | Payload offsets from byte 28 |
|---|---|---|
| `0x10` BEACON | Group `0xFFFF`, sequence 0 | `0..3` period in µs, `4` roster count 1–8, then `n` entries of Node ID (u16) and maximum application bytes (u8, 0–64). Index is table order. |
| `0x13` PAYLOAD_REQUEST | Unicast gateway, low 16 bits of frame ID | `0..3` gateway boot ID, `4` exact pending application length. The source is the assigned client. |
| `0x14` SLOT_GRANT | Group `0xFFFF`, sequence 0 | `0` roster count, then `n` entries of exact granted length (u8) and client boot ID (u32). `0xFF` and boot 0 indicate an absent request. |
| `0x11` DATA | Unicast gateway, low 16 bits of frame ID | `0..3` gateway boot ID then 0–64 application bytes. The client ID and client boot ID are in the header. |
| `0x12` ACK_BITMAP | Group `0xFFFF`, sequence is round 0–3 | `0` roster count, `1` accepted mask, `2` next retry mask, then one u32 client boot ID per roster entry. Positive positions contain the granted client's boot ID. All other IDs are zero. |

Bitmap index 0 is the least significant bit. Unused bits are zero. Accepted
and retry masks cannot overlap, retry grants must be among active grants and
fit the pool, and round 3 carries no retry grants. Clients require the same
network, gateway, gateway boot, frame, schedule, position and round. The
grant binds each position to an exact length and requester boot ID. v1, v2
and v3 decoders each accept their own wire version.

### Calendar and admission

![Four-client v3 calendar and bitmap example](../research/figures/v3-superframe.svg)

Each beacon lists eligible clients in fixed order. Clients with pending data
send length requests in their individual request slots. After receiving the
requests, the gateway broadcasts an exact-length grant. An empty request
phase produces no grant. The gateway and clients use serviced Beacon and
Grant completion timestamps as local phase anchors. They reject stale
beacons, retire the last gateway session after expiry, and require a current
grant to send DATA. A valid beacon and grant supply one frame of permission.

Let `G = 50 ms` be the provisional guard and `T(L)` the fixed SF7 airtime
equation in the v2 section below. A packet with `b` application bytes consumes
`T(32+b) + G` per DATA slot. A request slot consumes `T(33) + G`. Requests
begin `G` after the beacon completion. The grant follows the last request
slot after another `G`. The DATA calendar starts `G` after the grant
completion. Round 0 reserves all exact-length grants. Later rounds reserve
the four longest active slots, or all slots if fewer than four are active.
Before and after every aggregate ACK, the calendar inserts `G`. A launch
must begin within the first `G/4` of its slot. The receiver checks the
completion around expected airtime using the half-open `[-G/2, +G/2)` window.
The gateway abandons a late control completion whose remaining phases would
extend into the next beacon.

Admission computes the worst-case roster reservation before starting. A zero
period in `include/research_roster.h` selects the reservation rounded up to
milliseconds, with a floor of the **1,000 ms** target and a **10,000 ms** cap.
An explicit period must fit the reservation. The supplied eight-client table
with 64-byte budgets selects **6,516 ms**. Only requested lengths enter the
active DATA calendar. Both periods are separate from the **3,000 ms** report
default and the **1,000 ms** M2 ACK timeout. The example source has one pending
report per board, so an application may observe a slower on-air cadence.

### Payload ownership and outcomes

`FirmwareRuntime::submitPayload(bytes, size)` copies 0–64 bytes into a
single pending buffer using `PayloadPort`. It reports queued, busy, invalid
pointer, oversized or inactive. The input may be released on return. A
valid empty payload consumes the slot and ACK as usual. Pending bytes remain
immutable through all attempts. `payloadResult()` records accepted, missing,
unknown or excluded by the roster. A missing request or grant leaves an
unsent payload queued for the next eligible frame. An exhausted transaction
ends without silently resubmitting its bytes as a new logical packet.

The gateway calls the configured receiver synchronously in the loop. Its
byte span is valid only for that call. Return `true` after accepting the
packet or `false` to refuse it. The sample sink queues a hex record on the
serial logger and refuses delivery if that queue is full. The default client
example submits eight known bytes using the same API. Embedded applications
can supply their own source and bounded receiver when constructing the
runtime. The gateway invokes its receiver at most once per admitted identity
inside the current frame. It checks duplicates and conflicting bytes
separately. There is no persistence across reboot or application-level
acknowledgement beyond the local callback accepting the bytes.

The gateway grants retry slots only to missing positions and rotates the
selection start by frame and round. The pool may grant fewer than three
retries to a client when many peers are missing. A client that misses the ACK
does not know its next slot and stays silent until a later valid grant. A
later cumulative ACK can still confirm previously accepted DATA. Four
feedback rounds are emitted for an active frame to support this recovery.

### Ten-second analytical capacity

With equal 64-byte payloads, four shared retry slots per retry round, fixed
SF7 and a 50 ms guard, the results are as follows. The model reserves all
eligible requests, one beacon, one grant, primary DATA and up to four
feedbacks. It uses no measured timing or loss. The eight-client production
cap applies before admission. Wider bitmaps for larger rows describe a
hypothetical format and need their own protocol review.

| Clients | Calculated reservation | Grant bytes including header | Result |
|---:|---:|---:|---|
| 1 | 1.990656 s | 34 | Supported, exceeds 1 s by 990.656 ms |
| 2 | 2.999696 s | 39 | Supported |
| 4 | 5.022896 s | 49 | Supported |
| 8 | 6.515504 s | 69 | Supported |
| 10 | 7.274608 s | 79 | Fits time and packet bounds in the extrapolation, exceeds the implemented roster cap |
| 20 | 10.998448 s | 129 | Exceeds 10 s by 998.448 ms and exceeds the 96-byte limit |
| 70 | ≥27.020528 s | 379 | Lower bound omits unencodable control frames. Exceeds both limits |

At eight clients and 64 bytes the reservation ranges from 5.540504 s with
25 ms guards to 7.490504 s with 75 ms guards. A one-client, eight-byte
reservation still takes 1.662976 s at 50 ms. The 63-case model is
reproducible with `python tools/model_multiclient.py`. IRQ service delay,
radio BUSY, actual turnaround and clock drift remain unmeasured. The
calendar rejects late software launches, but the radio dependency can
still block after the check. See [ERR-021 and ERR-023](ERROR_LOG.md).

## Legacy protocol v2 (Iteration 019)

Build `heltec_wifi_lora_32_V3_research` for **both** boards. The saved NVS
role selects the gateway or client. `status` reports `profile=scheduled` and
`protocol=2`. Set network 1, gateway 1, client 2, and
`config set hello-interval-ms 10000` on both boards. Config storage and the
115200 baud console are shared with M2. An incompatible role/ID or interval
that cannot fit the schedule leaves the radio disabled. The scheduled build
has no OLED or protocol negotiation. Two nodes on separate wire profiles may
still contend for airtime, so use one profile per bench session.

## Wire layout

The v2 codec reuses the 28-byte envelope and 96-byte bound in
[PROTOCOL.md](PROTOCOL.md). Byte 2 is `2`; flags at byte 4, config version at
22–23, and reserved byte 27 are zero. Network and destination must match the
receiver. Source and destination are distinct unicast IDs. Sender boot ID
at 12–15 and gateway-assigned frame ID at 16–19 are nonzero. Schedule version
at 24–25 equals `1`. Frame IDs increase within each gateway boot session and
the gateway renews its session before reusing a frame number. All multibyte
fields are little-endian. The byte count at 26 must match exactly.

| Type at byte 3 | Header sequence at 20–21 | Payload offsets relative to byte 28 |
|---|---|---|
| `0x10` BEACON | `0` | `0..3` period (u32 µs), `4` maximum application bytes (0–64), `5` total rounds (`4`). Exact length 6 bytes. Destination 2 grants schedule index 0. |
| `0x11` DATA | Low 16 bits of frame ID | `0..3` echoed gateway boot ID (u32), `4..end` opaque bytes (0–64, bounded by grant). Exact length 4–68 bytes. The header sender boot ID identifies the client. |
| `0x12` ACK_BITMAP | Round index (`0..3`) | `0` active count (`1`), `1` bitmap byte count (`1`), `2` index-0 result (LSB, upper seven bits zero), `3..6` accepted client boot ID (u32), `7..8` accepted DATA sequence (u16). Exact length 9 bytes. Both identity fields must be zero when the bit is zero. |

Other types and versions are rejected. The active wire layout supports one
client. The bitmap length/padding validator also handles 10 and 70 logical
positions (2 and 9 bytes); that helper does not extend v2 airtime or capacity.

## Calendar, timing and retransmission

The gateway anchors a frame to serviced beacon TX_DONE. The client anchors
to serviced beacon RX_DONE, after checking sender, period, monotonic frame
ID and schedule version. It requires a fresh beacon for each frame. It
rejects a changed gateway session until the last beacon's period plus guard
has elapsed and remembers the most recently retired gateway session. That
history is volatile. A freshly rebooted client has no prior replay history.

The fixed SF7/BW125/CR4/5 airtime for `L` on-air application-frame bytes is
`T(L) = 256 µs × (81 + 20 × ceil((8L + 16) / 28))`. This accounts for
eight preamble symbols, the explicit header and PHY CRC. With `G = 50 ms`,
`D = T(32 + granted_payload)` and `A = T(37)`, round `r` begins at
`data(r) = 2G + r(D + A + 8G)`. Feedback begins at `data(r) + D + 4G`
and closes at `feedback(r) + A + 2G`. A sender can start only within `[start,
start + G)`. Receiver completion windows start at `start + airtime − G` and
end before `start + airtime + 2G`. The configured period must fit the complete
calendar, beacon airtime and end margin. The four feedback windows also
fit within the legacy 10 s v2 feedback budget. See the current M2 timeout in
[PROTOCOL.md](PROTOCOL.md).

The firmware currently grants eight bytes and sends its local beacon
observation time in microseconds. The gateway accepts the first valid DATA
in the assigned slot and holds its bytes and identity for the frame. A
byte-identical retry is acknowledged again. A conflicting payload or
session is rejected. The client stops after matching positive feedback,
retries after a valid zero bit (`missing`), or retries after the scheduled
window closes without matching feedback (`unknown`). All retries use the
same encoded DATA. After the initial transmission and up to three retries,
the client records `retry_exhausted`. The gateway sends cumulative feedback
in every round because it cannot know which feedback reached the client.

## Bench evidence still needed

`ScheduledLink` runs in `FirmwareRuntime` over `RadioDriver`. Both roles
use SF7, one radio and a fixed calendar. The 50 ms guard is experimental.
Clock drift, IRQ service jitter, radio switching, and the time from launch
request to on-air TX need physical measurement. The existing BUSY and flash
latency risks still apply. There is no authenticated replay protection,
multi-client allocation, general application delivery API, automatic PHY
retune or gateway-side ADR. See the [research traceability matrix](../research/ALGORITHM_TRACEABILITY.md)
for the paper assumptions and host evidence.
