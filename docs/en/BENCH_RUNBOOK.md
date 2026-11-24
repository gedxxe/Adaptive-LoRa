# Setup and field test

[Home](../../README.md) · [Protocol](SCHEDULED_LINK.md) · [Results](PROGRESS.md) · [简体中文](../zh-CN/BENCH_RUNBOOK.md)

## Before powering a board

Use a Heltec WiFi LoRa 32 V3.4, a suitable 50-ohm antenna or load and a
stable regulated 5 V supply with at least 500 mA input capability. Attach
the antenna or load before power, even on a single board. A saved role can
start transmission at boot. Check the physical RF variant, connector and
antenna markings. Use an authorized RF setup for radiated tests.

Connect one board directly using a USB-C **data** cable. Verify its CP210x
port appears and disappears in Windows Device Manager and
`pio device list`. Check **Ports**, **Universal Serial Bus devices** and
**Other devices**. Record the alias, VID/PID and COM port. If the bridge
appears without a port, use the [Silicon Labs driver](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers).
Wait for a confirmed port before upload or capture.

## Back up a board that has firmware or settings to keep

Before opening the serial monitor, hold PRG/BOOT when connecting power to
enter ROM download mode and release after the strap is sampled. If it is
already powered, hold PRG/BOOT and briefly press RESET. Verify the real
button labels. Keep the RF load connected. Use observed ports, esptool
5.x and a private directory outside Git.

```powershell
$python = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$esptool = Join-Path $env:USERPROFILE '.platformio/packages/tool-esptoolpy/esptool.py'
$port = 'COMx'
$dir = 'PRIVATE_DIRECTORY_FOR_THIS_BOARD'
& $python $esptool --chip esp32s3 --port $port --before no-reset --after no-reset --no-stub get-security-info
& $python $esptool --chip esp32s3 --port $port --before no-reset --after no-reset --no-stub flash-id
```

Proceed only if security permits reading and flash capacity is 8 MB.
Stop on a tool error or an unexpected layout. Avoid force, erase and
eFuse changes. Read twice using unique filenames.

```powershell
& $python $esptool --chip esp32s3 --port $port --before no-reset --after no-reset read-flash 0x0 0x800000 (Join-Path $dir 'flash-a.bin')
& $python $esptool --chip esp32s3 --port $port --before no-reset --after no-reset read-flash 0x0 0x800000 (Join-Path $dir 'flash-b.bin')
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $dir 'flash-a.bin'), (Join-Path $dir 'flash-b.bin')
```

Require two 8,388,608-byte files with matching hashes. Compare the actual
partition map and NVS range with the image you will upload. An ESP32 SDK
startup path can format NVS before the application runs. Keep a private
same-board restore plan. See [ERR-023](ERROR_LOG.md).

## Flash and set roles

Close any open monitor and replace `COMx` with the board's actual port.
Flash the same adaptive image on all participating boards.

```powershell
pio device list
$port = 'COMx'
pio run -e heltec_wifi_lora_32_V3_adaptive -t upload --upload-port $port
pio device monitor --port $port --baud 115200
```

**Gateway**

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
```

**First Client**

```text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
```

Give additional Clients unique node IDs 3–9. Check `config show`, wait for
`CONFIG_SAVED`, send `reboot` and run `status`. Verify the role, IDs,
`protocol=5` and `profile=adaptive` on every board. Replace earlier
adaptive images on both roles before the run. A new Client skips its Request
if the Gateway still advertises a 16 B ceiling. The configured interval controls
when the Client queues a sample. The over-the-air period comes from the Beacon.
The Gateway must report `period_us=15000000`, `roster_count=8` and
`bench_payload_ceiling_bytes=32` in `SCHEDULE_CONFIG`.
Check `data_sf_policy=adaptive` before the walk. The Gateway reports
`params_version=5` in its link samples. Record the flashed commit for all
boards before moving them to the route.

With Client 2, the first sample is `ID=0002|SEQ=0001|MSG=HELLO WORLD`.
Both fields use four hexadecimal digits. The sequence starts at one after
boot, advances when a new sample enters the queue and wraps after `FFFF`.
The DATA frame carries exactly 32 ASCII bytes, with no terminator or newline.
A retry sends the same bytes and packet identity.

<a name="check-the-pair-and-walk-the-route"></a>

## Verify delivery and link recovery

1. Run Client 2 close to the Gateway for at least five complete frames. Match
   Gateway `APP_PAYLOAD` with Client `V3_PAYLOAD_RESULT,result=accepted`
   for the same frame. Check `bytes=32` and the Gateway's hexadecimal payload,
   then check `ACKED DATA SF` on the Client. In a separate
   run, power-cycle the Gateway and check that the Client reconnects.
2. Add stationary Clients 3–5 and collect at least five complete frames.
   Then add Clients 6–9 and collect at least 20 complete frames with all
   boards stationary. The bring-up gate is that every ID delivers and
   acknowledges DATA, and there are no duplicate application deliveries,
   unexpected resets, missing serial streams or unexplained timing faults.
3. In a separate run, move Client 2 while keeping the Gateway and peers
   fixed. Record departure and stop times, antenna orientation and obstacles.
   Wait at least six complete frames after a change in SF to observe lock or
   rollback. Track peer delivery alongside `ADR_APPLY` and `V3_LINK_SAMPLE`.
   To check SF10, look for Client `TX SF10`, then `ACKED DATA SF10`, and
   continuing SF9 control exchanges. If SF10 is never granted, distinguish an
   over-budget profile (`adr=recovery_budget` or `adr=budget_limited`) from loss of SF9 control (`SEARCH`
   or missing Request/Grant). Keep this check separate from the acceptance run.
4. Record first-attempt and final delivery, SF, missed slots, resets and log
   drops per Client. A screen showing `SEARCH` is listening for a Beacon.
   An SF shown under `ACKED DATA SF` belongs to the last confirmed packet.
   Read `ADR_TIMING` with `status` before and after the run.

**Reading the display at a test point.** On every Client page, `TX SF8 CR4/5` means
the last DATA transmission started at SF8. It survives the switch back to
SF9 for control. On page 2, `ACKED DATA SF` changes only after a matching
ACK. At a stop, read both values and the AGE on page 1. Client AGE measures
time since a valid Grant or bitmap. An old TX value can persist through an
outage. `SEARCH` means the Client is listening for a fresh Beacon.
`NO GRANT` means its Request started but no matching Grant arrived.
`REQ FAIL` or `NO TX` reports a local send failure before DATA or a
granted frame with no DATA launch. On page 2, `APP MISSING` means that
feedback omitted the attempted DATA. `APP UNKNOWN` means its delivery
could not be confirmed. The Gateway shows `RX` for the last
valid DATA and identifies the sender on page 1. A dash means no DATA event
in this session. The display updates between radio phases.

To check a walk outage, hold position for six frames and record the
Client screen before moving again. Step back into SF9 coverage and check
the first new DATA attempt. The Gateway can restore the previous confirmed
SF or try the higher SF of its last accepted DATA in the same session
if the full roster still fits. Keep a stationary peer logging. To test
recovery with control still working, hold for up to ten frames so the
Gateway has time to detect the misses, try higher SFs and see a receipt.
Compare the moving node's `TX SF` and `ACKED DATA SF` with peer delivery.

**Matched SF7 comparison.** For a second run on the same route, change only
the Gateway image. Replace `COMx` with its observed port. This command uses
a child shell and a separate output directory:

```powershell
cmd /d /c 'set PLATFORMIO_BUILD_SRC_FLAGS=-DADAPTIVE_LORA_FIXED_SF_COMPARISON&&set PLATFORMIO_BUILD_DIR=.pio/comparison-build&&pio run -e heltec_wifi_lora_32_V3_adaptive -t upload --upload-port COMx'
```

Run `status` on the Gateway and record `data_sf_policy=fixed_sf7`. The
same Clients, SF9 control, 15 s period and retry rules apply. To restore
ADR, upload the ordinary adaptive image and check `data_sf_policy=adaptive`.

**Experimental ADR comparison.** For a matched EWMA run, build the
Gateway with the command below. Use `=2` and `.pio/adr-kalman` for Kalman.
Upload the corresponding Gateway image, verify its `adr_rule`, and use the
same Clients, route and offered load. The rule does not need provisioning.

```powershell
cmd /d /c 'set PLATFORMIO_BUILD_SRC_FLAGS=-DADAPTIVE_LORA_ADR_RULE=1&&set PLATFORMIO_BUILD_DIR=.pio/adr-ewma&&pio run -e heltec_wifi_lora_32_V3_adaptive -t upload --upload-port COMx'
```

## Test the rotating profile

Use the same build on the Gateway and all Clients.

```sh
pio run -e heltec_wifi_lora_32_V3_rotating
pio run -e heltec_wifi_lora_32_V3_rotating -t upload --upload-port COMx
```

Use the physical port and the [role commands](#flash-and-set-roles) above.
In `status`, confirm `protocol=6` and `profile=rotating`. With Gateway ID
1, the default registry contains Client IDs 2–129. For a small rotation
test, set `ADAPTIVE_LORA_REGISTERED_CLIENTS` in the build environment
for all boards. IDs 2 and 10 exercise separate groups with the default
registry. Capture at least two complete sweeps. `GROUP WAIT` with
`BEACON SYNC` is expected outside a Client's group. `SEARCH` means
it has stopped receiving Beacons on time. Use synthetic data only.
The [rotation model](SCHEDULED_LINK.md#rotating-schedule) gives the
expected schedule, and the capture must measure the actual delivery gap.
For larger tests, record all delivery events on the Gateway and use the
capture tool's eight Client ports for a sample of links across groups.

## Save the results

Close monitors before recording. Use the observed ports and a new
directory for each run. Use repeated `--client` arguments in node-ID order.
Use `--board COMx` to record just the Gateway.
Opening a serial session may reset a board. Check boot boundaries before
joining runs.

```powershell
$python = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$run = Join-Path $env:LOCALAPPDATA 'Adaptive-LoRa/captures/RUN-YYYYMMDD-HHMMSS'
Get-FileHash .pio/build/heltec_wifi_lora_32_V3_adaptive/firmware.bin
& $python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out $run
```

For eight Clients, use a new `$run` and the ports for node IDs 2–9:

```powershell
$ports = @('--gateway', 'COM_GATEWAY')
$clients = @('COM_ID2', 'COM_ID3', 'COM_ID4', 'COM_ID5', 'COM_ID6', 'COM_ID7', 'COM_ID8', 'COM_ID9')
foreach ($client in $clients) { $ports += @('--client', $client) }
& $python tools/capture_bench.py @ports --seconds 360 --out $run
```

Use the [run record](../../logs/test/RUN_RECORD_TEMPLATE.md). Save the
image hash, board aliases, slots and raw files. Keep raw logs, device
identifiers and private payloads outside Git. Mark Client ACK receipt
as unknown when only a Gateway log is available.

Make one CSV row per Client per complete frame. Suggested columns:

```text
run_id,gateway_boot,frame_id,node_id,client_boot,grant_seen,payload_bytes,data_sf,first_tx,first_received,app_accepted,client_ack,attempts,rssi_dbm,snr_db,rx_us,timing,segment
```

Join `V3_LINK_SAMPLE`, `APP_PAYLOAD` and the Client `V3_DATA_TX` and
`V3_PAYLOAD_RESULT` events within one Gateway boot. Start a new segment after
any boot or intentional outage. Keep absent or ambiguous observations unknown.
Exclude partial boundary frames. `class=ABSENT` means the Gateway saw no
valid Request. A missing log row alone cannot establish what happened.
`timing=invalid` identifies a local
schedule fault and is excluded from RF-only comparisons, while any recorded
application delivery still counts in the end-to-end result. `rx_us` is
Gateway uptime at reception, so use it for per-Client gaps within a boot.
For `adr=rx_window_unobserved`, the Gateway could neither establish that
it was listening at the assigned SF before launch nor verify DATA in that
slot. A sample with `class=PRIMARY_MISS` records a usable primary
receive window with no DATA even if timing elsewhere in the frame is
invalid. The Gateway can use that evidence for a bounded SF trial.
`adr=loss_unconfirmed` means the Gateway missed all DATA and the current
Request did not indicate low margin. Match the
Client's `V3_DATA_TX` with the Gateway sample by session, frame and node
before attributing the loss. `step_up` and `recovery_probe` open temporary
trials. `recovery_next` requests the next candidate. Match the Grant to
the Client's `V3_DATA_TX` to see what it used. `recovery_confirmed` records
a receipt matched during Request processing. `recovery_budget` means the
trial could not fit, `recovery_cooldown` marks the wait after rollback,
and `ADR_PROBE` with `result=expired` marks an unconfirmed trial that
reached its deadline. Record both RSSI and SNR alongside the delivery
result when interference is suspected.

Probe DIO1, NSS, BUSY and SCL with high-impedance probes. Measure at least
30 transitions per SF pair used, including both directions and return to
control SF. Check the observed tail and worst transition against the 20 ms
retune lead and 40 ms receive half-window. Record drift over complete
15 s frames. A missed deadline or unexpected reset merits investigation
before using the walk test to compare RF performance.
