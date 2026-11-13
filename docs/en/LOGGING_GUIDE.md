# Logging Guide

## Serial format

Use one structured line per event. CSV is acceptable for the first
firmware version, provided commas in text fields are escaped or avoided.

Recommended fields:

`uptime_us,role,node_id,event,frame_id,sequence,boot_id,config_version,schedule_version,sf,bw_khz,freq_mhz,rssi_dbm,snr_db,toa_us,retry_count,state,result`

Use `NA` for a field that is not applicable or unavailable. Do not use
zero to mean unknown.

The M2 implementation emits comma-separated `key=value` fields, one event per line, and uses `N/A` for unavailable fields. Frame events include network/source/destination/session/sequence and version fields. `toa_us` is the RadioLib calculation for a TX start, not measured latency. `LINK_COUNTS` includes attempts, accepted ACKs, ACK timeouts, TX failures and transaction duration. `ACK_TX` means start; `TX_DONE` confirms completion. `status` reports the 16-line queue's `log_dropped`; preserve zero-drop capture for acceptance. The bounded queue drains at most 128 bytes per loop and never prints in ISR.

Audit correction (ADR-010/ERR-010): queue processing and requested byte count are bounded, but installed SDK availableForWrite/write can wait on mutexes/ring-buffer progress. Do not describe end-to-end serial I/O as proven nonblocking. Native backpressure/short-write tests exercise queue bookkeeping only, not USB/UART latency.

## Required events

- `BOOT`
- `CONFIG_VALID`
- `CONFIG_INVALID`
- `PROVISIONING`
- `RADIO_INIT`
- `RADIO_ERROR`
- `TX_START`
- `TX_DONE`
- `RX_DONE`
- `RX_TIMEOUT`
- `FRAME_REJECT`
- `HELLO_RX`
- `ACK_TX`
- `ACK_RX`
- `ACK_TIMEOUT`
- `DUPLICATE`
- `BEACON_RX`
- `SLOT_START`
- `SLOT_END`
- `ACK_BITMAP_RX`
- `RETRY`
- `ADR_PROPOSE`
- `ADR_ACCEPT`
- `ADR_COMMIT`
- `SYNC_LOST`
- `REJOIN`
- `WATCHDOG_RESET`

## Log quality

- Include a sequence or frame identifier where applicable.
- Include the RadioLib status code for driver failures.
- Do not print from ISR context.
- Do not block the radio state machine to format large messages.
- Keep a raw serial capture for each physical RF test.
- Reference the capture path from `BUILD_MEMORY.md` and the
  corresponding issue from `ERROR_LOG.md`.

## Iteration013 capture integrity and CSV extraction specification

Capture schema2 is host evidence metadata, not a firmware/protocol version. Default sends `status` at open and once after requested duration, then reads a1s tail; no configuration writes. `--board` selects one port; `--gateway/--client` require distinct ports; `--no-status` is passive. DTR/RTS=false is requested before open, but reset/glitches/disconnects remain possible. Use PlatformIO Python and a new run directory; no live monitor may share a port. Metadata can include a <=64KiB operator JSON object containing actual flashed binSHA/commit, unit aliases/revisions/config/PHY/instrument/authorization references. Do not mistake capture-script SHA or a planned configuration for verified target firmware.

`<stream>.log` retains exact bytes; `<stream>.chunks.jsonl` records each received chunk's raw offset, length, UTC and monotonic_ns; `capture.json` stores lengths, SHA256, newline/tail counts and state/errors. Reads are4096 bytes, metadata bounded, no line or packet history accumulates in memory. Record initial RUNNING before port open; completed acquisition uses COMPLETE, Ctrl-C INTERRUPTED/exit130, failures ERROR/exit1, invalid CLI exit2. A hard-killed run can retain RUNNING. Exit0 only means acquisition completed, never RF acceptance. Missing/removed ports, short writes, unjoined workers, disk/metadata errors are explicit failures. Workers have read0.2s/write1s timeout and3s join budget each; an underlying blocked driver may still need process/device intervention. Never use unfinalized hashes/lengths from unjoined writers.

Before extraction, verify COMPLETE/error-free state, matching raw lengths/hashes, contiguous chunk offsets ending at raw length, timestamp ordering and sidecar parseability. This detects recorded-file truncation/mismatch, **not USB/UART loss before read**. Empty streams, partial final lines and first-capture fragments need review; no partial line is silently made into an event. Preserve all raw bytes, decode complete lines strictly, flag malformed/non-event/overlong lines (>512 firmware bytes) and unknown fields. A final fragment may be excluded only outside an explicitly closed analysis window; it cannot justify treating an incomplete transaction as an RF loss. An errored/interrupted acquisition must be repeated for primary acceptance.

Write an events CSV with these columns (empty/N/A stays unknown, never numeric zero):

```text
run_id,stream,raw_start,raw_end,observed_utc,observed_monotonic_ns,
cohort,device_boot_index,uptime_us,network_id,node_id,event,result,code,
source_id,destination_id,boot_id,sequence,hello_key,association_status,
rssi_dbm,snr_db,toa_us,attempts,ack_success,timeouts,tx_failures,
transaction_us,log_dropped
```

Each CSV row maps to a complete raw line and the chunk containing its ending byte. Host timestamps describe **read observation**, often shared by coalesced lines; they are not air/IRQ timestamps or synchronized MCU clocks. A missing field remains blank. BOOT/uptime rollback/operator reset splits device sessions; ambiguous reset/capture boundaries invalidate associations rather than silently joining sessions. BOOT counts are observed lower bounds, not an independently measured reset count.

Logical HELLO key K=(network_id, client source_id, client boot_id, HELLO sequence). Keep raw event identity separately: Gateway ACK header sequence/boot belong to the Gateway, not K. Association rules:

| CSV event family | Extraction/association rule |
|---|---|
| HELLO attempts A | Distinct K from Client TX_START/result=hello **or** HELLO_FAILED/result=tx_start_failed. Failed starts have no TX_START; do not omit them |
| HELLO completions T | Client TX_DONE/result=hello_sent for K; means driver-reported completion, not independently measured RF emission |
| Unique Gateway receives R | Valid HELLO_RX/accepted or DUPLICATE/reack for K in the Client cohort, unique K once. Report HELLO_RX new markers and DUPLICATE events separately |
| Duplicate packets | Count DUPLICATE events and unique duplicate K separately; same-K replay does not create a new application delivery or normal-M2 automatic retry |
| ACK starts | Gateway TX_START/result=ack is own header identity; adjacent ACK_TX/started echoes K. Require both/order and a unique receive/start association; do not count both as two starts |
| ACK completions | Gateway TX_DONE/result=ack_sent matched to Gateway TX_START header identity, then K. No completion may be inferred from ACK_TX alone |
| Accepted ACKs C | Client ACK_RX/result=accepted attached to its single outstanding K, with subsequent LINK_COUNTS increment. Header fields identify Gateway; firmware validates echoed payload. Logs alone do not independently dump/check the entire ACK payload |
| Timeouts/failures | ACK_TIMEOUT and HELLO_FAILED keyed by outstanding K; preserve result/code and distinguish tx_start_failed/tx_failed/timeout |
| Radio errors | Keep event/result/code and stage; one failure may emit RADIO_ERROR plus HELLO_FAILED, so event counts are not independent packet-failure totals |
| RSSI/SNR | Valid Gateway HELLO_RX/DUPLICATE and Client acceptedACK metrics, separated by direction/new/duplicate. Avoid double-counting the preceding RX_DONE formatter record |
| Latency | LINK_COUNTS.transaction_us associated with the immediately completed Client transaction. Split success/timeout/TX-failure distributions; it runs from post-startTransmit sentAtUs to finish, excludes start-call time, and is not one-way/turnaround latency |
| Log drops | Initial/final STATUS.log_dropped for each stable boot; require relevant counters0 and no missing snapshots/reset ambiguity. A newly reset0 is not proof of an earlier zero |

Build a transactions CSV with one row/K and booleans/unknown for api_attempt,tx_started,tx_done,gateway_valid,duplicate_count,ack_started,ack_done,ack_accepted,terminal_result,transaction_us and error references. Count only fully observed, declared start/end cohorts. Reconcile Client LINK_COUNTS deltas for attempts/ack_success/timeouts/tx_failures at closed transaction boundaries. Report excluded boundary/ambiguous records and any receives/ACKs outside the cohort; do not repair missing logs by assuming RF outcomes.

Denominators:

- Physical normal-run minimum: >=200 fully observed Client HELLO completions |T|; report all A and start failures as well.
- PDR = |R intersect T| / |T|. Also report total observed R and received-without-observed-T anomalies; unpaired records cannot make PDR exceed1.
- Transaction ACK success = |C| / |A|, including start failures in A; accepted-over-completed = |C intersect T| / |T| is a separate optional ratio.
- Conditional Gateway ACK-start/completion counts and rates must state matched R or starts as denominator. Keep starts and completions separate.
- Duplicate-event ratio uses received valid events (new markers+duplicate events) as denominator, with cohort scope stated.
- Zero denominator -> N/A. No silent numerator clipping, cumulative reset mixing, CRC-failure total inference or automatic selection of a favorable window.

The runbook's existing >=99% short-range target applies to declared transaction success C/A and is an unvalidated engineering acceptance target, not industrial reliability. RF injection/timeout interventions are separate runs, never mixed into the normal cohort. Capture/tool unit tests and native simulations cannot be included in physical denominators.

Future BEACON/slot/bitmap/ADR event names listed earlier are reserved for later milestones; their absence is not an M0-M2 capture failure.
