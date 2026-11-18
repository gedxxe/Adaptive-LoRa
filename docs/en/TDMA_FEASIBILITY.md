# TDMA feasibility and M3 design notes

Iteration 015, host-only preliminary research. Production remains firmware
0.2.0-alpha.1, protocol 1, fixed-PHY HELLO/ACK. No BEACON, DATA, bitmap,
scheduler or ADR code is activated by this work. M3–M8 production is NOT STARTED.

## What the model calculates

[tdma_feasibility.py](../../tools/tdma_feasibility.py) uses Python standard
library arithmetic and has no hardware access. Its inputs include a list of
per-client application payload lengths, data/control PHY, beacon airtime,
ACK strategy, zero to two retry opportunities, switching/processing/guard
allowances and superframe duration. Different client payload lengths are
supported by the API; the CLI repeats one payload length for all clients.

For SF7–SF12, with normal coding CR4/5 through CR4/8:

```text
Tsym_us = 1,000,000 * 2^SF / BW_Hz
DE = 1 when Tsym_us >= 16,000, otherwise 0 (or an explicit override)
IH = 0 for explicit header, 1 for implicit header
Npayload = 8 + max(ceil((8*PL - 4*SF + 28 + 16*CRC - 20*IH)
                       / (4*(SF - 2*DE))), 0) * CR_denominator
Tpacket_us = ceil((preamble + 4.25 + Npayload) * Tsym_us)
```

PL includes the 28-byte Adaptive-LoRa header. CRC is a boolean PHY setting,
not two extra application bytes. Integer/Fraction arithmetic rounds coding
blocks upward and complete packets upward to microseconds. The model restricts
bandwidth to 125/250/500 kHz, where symbol times are integral microseconds,
and deliberately excludes SF5/6, long interleaving and other modems.
Only SF7/SF8/SF9 at BW125 are used in the research grid; firmware stays SF7.

These equations follow the pinned [RadioLib 7.8.1 SX126x airtime implementation](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/SX126x/SX126x.cpp)
and its [automatic LDRO selection](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/SX126x/SX126x_commands.cpp).
The first source cites section 6.1.4 of the SX1268 datasheet. This is a
source-supported analytical calculation, not a measured radio duration.
Tests include independently worked symbol/block vectors and boundary cases.

| Application bytes | Complete bytes | SF7/BW125 | SF8/BW125 | SF9/BW125 |
|---:|---:|---:|---:|---:|
| 68 | 96 | 164,096 us | 297,472 us | 533,504 us |

### Accounting assumptions

One initial round reserves every client's DATA and its acknowledgments.
Each retry opportunity reserves another complete worst-case round, including
acknowledgments; it is not a probabilistic expected retry rate. One beacon
is charged per superframe. Individual ACK is the existing 28+9=37 bytes.
Planned bitmap ACK is 28+2+ceil(N/8) bytes: 31/32/39 bytes for 1/10/70
clients, including active-count and bitmap-length metadata. Mapping/version
come from the schedule and existing envelope. Both ACK strategies use an
explicit control PHY input, SF9 in the grid; current M2 actually ACKs on SF7.
The comparison is a future schedule hypothesis, not a firmware change.

The gateway action count is `1 + rounds*(N + ACKs_per_round)` for beacon TX,
uplink RX windows and ACK TX. Each action is charged one preparation/switch
allowance and one processing allowance; each uplink window is charged one
guard allowance. These deliberately simple serial gateway costs must be
replaced or calibrated against a measured state-transition trace before M3.
They do not model overlapping client preparation, contention, interference,
regulatory airtime budgets, synchronization acquisition or schedule transfer.

Unknown beacon/switching/processing/guard inputs remain JSON null. A known
occupancy floor above the superframe proves INFEASIBLE under the assumptions.
Otherwise unknown costs yield UNDETERMINED. Fully specified costs can yield
CONDITIONAL_FIT, which means arithmetic fit only. `total_occupancy_us` and
`remaining_us` remain null if any cost is unknown; missing values are never
silently treated as measured zero. Negative, noninteger and out-of-range
inputs are rejected. No claim of RF reliability follows from a fit.

### Reproducing the grid

```sh
python -m unittest discover -s tools -p test_tdma_feasibility.py -v
python tools/tdma_feasibility.py --scenarios --out logs/test/iteration-015-tdma-scenarios.json
```

The 216 scenarios cover clients 1/5/10/20/35/70, SF7/8/9, application payloads
16/68 bytes, retries 0/1/2 and individual/bitmap ACK. All use a **hypothetical
10-second superframe**, SF9 control and a 45-byte cached-schedule beacon
(308,224 us). All switching/processing/guard costs are unmeasured/null.
The beacon hypothesis assumes the complete versioned schedule is already
installed. Distribution/rejoin traffic is excluded; the grid therefore does
not establish enrollment feasibility or 70-client capacity. Interpret the
machine-readable report with those assumptions, not as physical simulation.

Even 70 maximum-size uplinks alone consume 11.487/20.823/37.345 seconds at
SF7/8/9, before beacon, ACK or guards. No-retry SF7 bitmap at that size cannot
fit the hypothetical ten seconds. Smaller payloads/counts that remain below
ten seconds still have an unknown timing margin. Bitmap savings must be
compared using the full downlink frame and control PHY, not only bit count.

## Proposed M3 contract, not a wire-format implementation

Retain the v1 28-byte envelope as a candidate. BEACON 0x10 is reserved today;
M2 rejects it, nonzero frame/schedule fields and broadcast destinations.
Future support requires type-specific validation, mixed-version behavior and
a versioning review before freezing bytes. Merely allocating a type did not
make old clients compatible. Keep HELLO/ACK golden bytes and schema 1 intact;
if new semantics make operation incompatible, bump the protocol as required
by [VERSIONING.md](VERSIONING.md). No version changes occur in Iteration 015.

A **candidate**, little-endian M3 beacon payload is:

| Field | Bytes | Meaning |
|---|---:|---|
| format_version | 1 | Proposed beacon payload revision |
| active_client_count | 1 | Enrolled clients in the referenced schedule |
| entry_count | 1 | Number of entries carried here |
| superframe_duration_us | 4 | Frame period |
| lease_frames | 2 | Upper bound on schedule validity; zero rejected |
| uplink_start_after_beacon_us | 4 | First RX slot relative to beacon completion |
| slot_duration_us | 4 | Complete one-client slot budget |
| entries | 6 each | node_id u16, profile_index u8, reserved-zero u8, slot_index u16 |

Frame ID, gateway session and schedule version use the envelope. The fixed
payload is 17 bytes. One entry gives 51 complete bytes; at most eight entries
fit the existing 68-byte payload (93-byte frame). Ten or seventy entries do
not fit. Later multi-client schedule caching/fragmentation and atomic
activation need a separate design; partial schedules must never authorize TX.
The 45-byte grid beacon carries zero entries and assumes a cached schedule.
For actual M3, start with one complete entry and explicitly validate count,
slot uniqueness, source, profile, duration, lease and the client's identity.

Gateway advertises frame ID, committed schedule version and relative slot
times. Client accepts only its expected gateway/network/session, complete
known mapping and increasing frame identity. A reboot/new gateway session
invalidates old timing and leases. Version changes activate atomically at an
agreed future frame; stale/unknown/partially delivered schedules suppress
normal uplinks. Frame/version wrap needs defined serial-number comparisons
and bounded leases; do not infer order with ordinary unsigned greater-than.

Gateway TX completion and client RX completion are local anchors. Measure
DIO1/ISR-to-service delay, radio preparation, airtime reference uncertainty,
processing and relative oscillator drift over the longest beacon interval.
Prepare RX before the earliest valid arrival, then reserve packet airtime,
processing and guard inside each slot. No measured guard is available yet.
A missed beacon suppresses that frame's DATA in the conservative M3 proposal.
The lease bounds cached mapping validity, not permission to transmit without
synchronization. Repeated misses/expired lease enter UNSYNCHRONIZED then the
future M6 rejoin state; no unscheduled DATA or improvised rejoin TX is allowed.

Use extended 64-bit monotonic microseconds for absolute frame anchors and
long superframes, plus checked relative offsets. A uint32 microsecond timer
wraps at 4,294.967296 seconds; signed modular deadline comparison is valid
only within 2,147.483648 seconds of the reference. Current M2 deadlines obey
that half-range. Future long leases/frame periods must not be truncated into
uint32 deadlines. Python integers do not validate MCU clock extension or ISR
atomicity. Keep one event-loop radio owner; no new task/timer framework is
justified before these timing measurements.

## Deduplication before M4/M8

Native tests characterize the actual SequenceTracker for 1/5/8/9/10/70
sessions, replay before/after eviction, boot churn, rollover and object
recreation. Eight retained sessions preserve their 64-sequence windows.
Nine or more keys replayed in insertion order continuously evict the next
key, so all replayed packets become NEW. Boot IDs compete for the same slots;
gateway reboot discards the entire volatile history. This is the documented
bounded M2 HELLO cache, not a newly fixed defect or a DATA exactly-once claim.
There is no automatic M2 retransmission; replay fixtures inject identical
captured bytes, while the next transaction after timeout uses a new identity.

For M4, prefer admission-bounded state per active schedule entry with an
explicit accepted session and replay window. Reject unknown sessions until
HELLO/rejoin admission; retain a bounded retired-session policy and expire
it deliberately. Schedule replacement and gateway restart require explicit
rejoin/epoch semantics before application delivery resumes. Do not fix this
by silently expanding the M2 round-robin array to seventy entries.

A provisional naturally aligned session record needs about 24 bytes
(network/source IDs, boot, highest sequence, flags/padding and 64-bit window).
Seventy records budget about 1,680 bytes, or about 2,240 bytes if each also
reserves eight bytes for slot/profile/admission metadata, excluding metric
windows, queues and retired sessions. These are design estimates, not a
newly implemented allocation. Current ELF type sizes are recorded separately
in the Iteration 015 resource probe. Linear scanning is O(N) per packet;
schedule-index lookup can be O(1) after validated identity/mapping. Neither
memory fit nor seventy logical tests proves radio capacity.

## Future M4–M6 contracts

Bitmap validation binds gateway/network/session, frame ID and schedule
version, exact count/length and zero unused high bits. Index i maps explicitly
to schedule entry i, least-significant bit first. Distinguish valid bit 0
(MISSING) from absent/invalid bitmap (ACK_UNKNOWN). Both may retry only in
the assigned deterministic retry position. Preserve source/boot/sequence and
original payload; ACK loss must not cause duplicate application delivery.
Reserve at most two retries initially and retain exhaustion diagnostics.

ADR remains gateway-controlled, SF7–SF9/BW125, fixed TX power and common SF9
control. Use rolling RSSI/SNR, PDR/PER and retry history, minimum observation
windows, hysteresis and a hold decision for ambiguous evidence. Thresholds
and window lengths remain experimental/TBD. CONFIG_PROPOSE/ACCEPT/COMMIT
bind config_version and effective_frame_id; commit only at a frame boundary.
A lost commit keeps the previous profile. The gateway must preserve a common
control/recovery path while resolving asymmetric state; a successful local
commit does not prove the peer received it. These contracts require loss,
stale-version and reboot tests before activation.

Next gate: finish M0–M2 physical acceptance and measure timing/memory before
reviewing an actual one-client M3 codec/scheduler change. See
[TEST_PLAN.md](TEST_PLAN.md) and [MILESTONE_TASKS.md](MILESTONE_TASKS.md).

Executed grid result: 103 INFEASIBLE, 113 UNDETERMINED; no physical capacity result.
