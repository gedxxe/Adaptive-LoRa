# Scheduled research link (v2)

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
fit within 10 s. M2 separately uses a full 10 s ACK timeout after HELLO.

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
