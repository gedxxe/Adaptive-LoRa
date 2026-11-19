# M0-M2 Heltec bench runbook

This procedure covers the reported Heltec WiFi LoRa 32 V3.4 pair.
It has not been executed on hardware. Read HARDWARE.md and the migration
audit first; source/build completion does not close physical acceptance.

## Before power or upload

1. Check each PCB’s V3.4 marking, MCU, radio, RF SKU and connector against the V3 reference.
   Record front/back markings, RF option, unit alias, cable and supply.
2. Connect a matching 50-ohm RF load or suitable antenna/pigtail before any
   power. This also applies to a single board: the radio is integrated,
   and an unknown factory image or saved valid role can transmit.
3. Identify PRG/RESET and CP2102 USB routing. Use manual ROM download mode
   for private backups before running unknown application code. Confirm
   security state and actual8 MB flash capacity. If secure/encrypted/read
   restrictions or incompatible layouts prevent safe backup, stop for a
   board-specific plan; do not change eFuses or force an operation.
4. Take two consistent full-flash reads while the same board is held in
   ROM; preserve hashes, partition table, NVS and a verified same-board
   recovery plan privately. Backups may contain credentials. Do not commit
   them or restore a different MCU/board image to this one. Provision old
   logical IDs through the console rather than copying an old full image.
5. Inspect the actual upload image offsets/erase ranges against the board's
   table. The current build expects NVS0x9000/0x5000 and app0x10000 with
   default_8MB partitions; these are build facts, not a read of a device.
   SDK startup can erase NVS after certain init errors before Runtime
   executes (ERR-023). A non-overlapping upload alone cannot promise NVS
   retention. Back up before the first application boot.

## Build and select the port

```powershell
pio run -e heltec_wifi_lora_32_V3
pio test -e native
# With PlatformIO's Python:
python -m unittest discover -s tools -p test_capture_bench.py -v
```

With only the intended board connected, compare USB enumeration/port
identity. CP2102/UART0 is the default path;115200 is a real UART setting.
Native USB CDC is disabled. Port opening/DTR/RTS may reset the board, and
upload requires exclusive use of that port. After backup/layout review,
future operator commands are:

```powershell
pio run -e heltec_wifi_lora_32_V3 -t upload --upload-port COM_BOARD
pio device monitor --port COM_BOARD --baud 115200
```

The upload and monitor commands remain pending for the two V3.4 boards. Keep RF
termination connected throughout. Do not erase all flash, flash a factory
image at0 in place of the reviewed upload images, or use force as routine
recovery. Any intentional NVS erase/corruption fixture needs explicit
operator intent, verified offsets and same-board backup/restore evidence.

## Provisioning and a terminated single-board check

A single Heltec still contains its radio. The old removable-radio MCU-only
procedure is inapplicable. Invalid/missing config must produce CONFIG_INVALID,
PROVISIONING and radio=disabled without normal TX; an existing valid config
instead initializes SX1262 and may transmit. Capture the actual state.

Set network/node/gateway IDs explicitly. Gateway commands:

```text
help
status
config show
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 15000
config save
status
reboot
```

For Client, use role client, node-id2, gateway-id1 and network-id1. Save
must validate and verify readback; staged changes must not change the loaded
role during a transaction. After controlled reboot/power cycle, verify the
loaded role/IDs/schema and configuration validity. With no valid role,
normal TX remains disabled. Exercise invalid IDs, inconsistent role IDs,
malformed commands and recovery to status without altering the last valid
configuration. Invalid staging is not evidence of corrupt-NVS boot handling;
that requires a separately backed-up physical fixture.

Confirm RADIO_INIT reports the selected SX1262 driver and actual status.
Measure supply, BUSY/DIO1/reset, TCXO startup, serial stability and actual
resets. A driver label is not an exposed silicon-ID measurement. If serial
progress stops, preserve the failed run, remove power safely with RF still
terminated, and inspect. Do not assume a default watchdog will recover
ERR-021 or that a successful normal run closes that fault path.

## V3.4 board and OLED checks

For each board, record alias, front/back revision and component markings,
RF SKU, antenna label/band, connector, port alias, flash size, supply and
firmware SHA-256. Keep private device identifiers and backups out of the
repository. Compare the [reference matrix](HARDWARE.md) before assuming V3.4
retains the OLED circuit, TCXO or RF switch. Check one board at a time.
Confirm a clean boot and reset, loaded NVS, selected SX1262 driver status,
and the actual supply voltage/current under load.

Use the default build for the reference comparison. After checking the
OLED pins, Vext polarity/settling and controller/address, build the candidate
with `pio run -e heltec_wifi_lora_32_V3_oled`. Use the same candidate image
for both roles. With a suitable load, check provisioning, radio-fault and
WAIT screens, use serial `status` to read OLED timing, and check whether a
missing/failed display leaves serial and radio service active. Do not drive
Vext or load this build onto an unconfirmed board. Compare power, resets,
log drops, ACK results and transaction duration with OLED on and off under
the same authorized link conditions. Record the firmware hash for each run.

## RF approval and interval calculation

The firmware retains 915.0 MHz as a research setting. For Indonesian LPWAN,
the [2025 amendment, Annex I section II](https://jdih.komdigi.go.id/produk_hukum/unduh/id/949/t/peraturan%2Bmenteri%2Bkomunikasi%2Bdan%2Bdigital%2Bnomor%2B2%2Btahun%2B2025)
lists 920–923 MHz with up to 250 kHz per channel and 1% each direction. The
compiled 915.0 MHz center lies outside that band. Confirm the applicable
rules, equipment authorization and an approved channel/EIRP/antenna setup
before radiating. Obtain a reviewed alternate build if the authorized
frequency differs. Software does not enforce duty cycle.

At SF7/BW125/CR4/5, preamble 8, explicit header and CRC, the host model gives
66.816 ms per 28-byte HELLO and 82.176 ms per 37-byte ACK. For one exchange
per period, the 1% minimum periods are 6.6816 s uplink and 8.2176 s downlink.
At 15 s the modeled fractions are 0.44544% and 0.54784%. At 5 s they are
1.33632% and 1.64352%. Budget starts after reset, extra duplicate ACKs, every
other emission and the applicable observation period. The 15 s candidate
is an interval calculation for a two-board run. Confirm both settings after
reboot, particularly the client interval. The gateway uses its interval
for display freshness.

## Two-device HELLO/ACK

After approval, start Gateway first, then Client. Begin at roughly 3 m with
both monopoles vertical. Record the approved frequency, BW125, SF7, CR4/5,
preamble 8, explicit header, CRC, sync 0x12 and TX power. Keep PHY, power,
monopole type, height, orientation, feed and duration fixed for comparison.
Record both OLEDs and check each receive direction in serial. Extend the
survey to 10, 25, 50, 100, 200 and 400 m only as conditions allow. Short
windows are exploratory. Use at least 200 fully observed HELLO completions
for each condition selected for acceptance.

Close interactive monitors before capture. Example future command:

```powershell
python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 3600 --out logs/test/RUN-YYYYMMDD-HHMMSS
# For one terminated board:
python tools/capture_bench.py --board COM_BOARD --seconds 180 --out logs/test/RUN-YYYYMMDD-HHMMSS-board
```

For separate hosts use `--board COM_GATEWAY` and `--board COM_CLIENT` with
matching run IDs and distinct suffixes. Start the loggers before the selected
cohort. At a 15 s interval, 1,200 s is too short for 200 completed HELLOs.
Allow at least 3,000 s plus startup, transaction time and closing margin,
and check the completed cohort count. Host observation clocks need not agree.

Use PlatformIO Python/pyserial. Every capture needs a new directory and a
completed RUN_RECORD.md copied from the current template. The utility sets
DTR/RTS intent before open but cannot prevent OS/board glitches. It sends
status at open and normal end, with a final tail; missing final counters
block acceptance rather than being reconstructed after a reset.

Require COMPLETE/error-free capture, contiguous raw offsets, matching
length/hash, stable boot boundaries and log_dropped=0 for the chosen window.
Preserve partial fragments and exclude only explicitly documented boundary
records. No duration alone proves200 packets. Select at least200 fully
observed HELLO TX completions T and separately count all API attempts A,
gateway unique receives R, duplicates, ACK starts/completions and accepted
ACKs C. PDR=|R intersect T|/|T| and ACK transaction success=|C|/|A|; failed
starts remain in A. Zero denominator is N/A. Follow LOGGING_GUIDE.md for
identity association, gateway-header versus echoed-client fields, counters
and CSV extraction. Host read timestamps are not synchronized radio clocks.

The provisional short-range engineering target is99% accepted transactions
on the stated denominator, not a field guarantee. Report both directional RSSI/SNR distributions and client transaction
duration with sample count, min, median, p95 and max. Mark absent metrics N/A.
Record ACK starts and completions separately, timeouts, TX failures, reset
boundaries, and log drops. Join using the HELLO identity and the echoed ACK
identity. Missing serial lines compromise capture completeness and cannot
be classified as lost RF packets. OLED screenshots are supplementary.
Report supply and exclusions as well. Reproduce timeout by controlled
reception loss, then restore and check later new transactions. M2 has no
automatic packet retry. Duplicate replay and wrong-identity/malformed ACK
checks need reviewed physical injection fixtures; native cases are separate.

## Acceptance

M0 needs actual board/USB/flash/power/antenna/RF conditions and toolchain
records. M1 needs both roles' radio/IRQ/BUSY/reset and NVS/serial behavior.
M2 needs the closed-cohort normal run and timeout/duplicate/negative-identity
and corrupt-config evidence. ERR-021 recovery remains a separate hold.
Leave unavailable measurements as NOT MEASURED. M3 begins only after M2
physical evidence and measured switching/clock guards exist.


## Planned ERR-021 fault injection — not executed

This requires a separately reviewed electrical fixture and attended bench
session after normal M0/M1 bring-up. Do not short the SX1262 BUSY output to
a supply or drive against it. On this integrated board, establish a buffered,
isolated MCU-side BUSY input path only after reviewing the actual PCB and
fixture. If that cannot be done without unsafe contention, defer injection.

1. Preserve same-board backups/security/layout, firmware hash and initial NVS;
   terminate RF with a suitable 50-ohm load and use current-limited power.
   Provide independent power removal and agree/record an external maximum
   test duration before starting (limit TBD, not a measured radio bound).
2. Observe NSS/SCK/MOSI, BUSY, DIO1, reset, supply/current and UART with
   high-impedance instrumentation at the correct logic voltage. Establish a
   normal TX trace first; do not infer setTx success solely from an SPI opcode.
3. Arrange a reviewed trigger so the MCU sees BUSY high specifically after
   setTx has returned success and before/during the separate ramp poll.
   Holding it high earlier may exercise Module's finite command timeout
   instead and is not reproduction of ERR-021. Trigger latency and electrical
   isolation must be characterized; no fixture is supplied or tested here.
4. Record whether loop/UART progress stops, elapsed time, DIO1, RF/supply and
   any watchdog/reset. A debugger can change watchdog behavior; report it.
   Stop at the external limit and safely remove all board power if progress
   is absent. Do not wait indefinitely for the application's poll deadline.
5. Remove the fault, verify normal wiring/power, restore the same-board state
   only under the backup/recovery procedure, and repeat normal initialization
   and HELLO/ACK. Preserve failed captures as well as recovery evidence.

Closure needs the precise fault reproduced and an approved bounded return or
independently measured recovery, including radio/PA shutdown and initialization.
A normal link run or source review cannot close ERR-021. No watchdog/library
modification or physical fault injection was performed in Iteration 015.
