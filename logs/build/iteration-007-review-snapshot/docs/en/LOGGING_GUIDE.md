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
