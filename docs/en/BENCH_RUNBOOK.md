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
`protocol=4` and `profile=adaptive`. The configured interval feeds the
sample application. The over-the-air period comes from the Beacon.

## Check the pair and walk the route

1. Run the pair close together for several complete frames. Match
   Gateway `APP_PAYLOAD` with Client `V3_PAYLOAD_RESULT,result=accepted`
   for the same frame. Check `ACKED DATA SF` on the Client. In a separate
   run, power-cycle the Gateway and check that the Client reconnects.
2. Add stationary Clients 3–5. Keep the Gateway and these Clients in place
   while Client 2 moves. Record departure and stop times, antenna
   orientation and obstacles. Watch for changes in `ADR_APPLY` and a later
   `V3_LINK_SAMPLE` with `locked=1` for that Client.
3. Record first-attempt and final delivery, SF, missed slots, resets and log
   drops per Client. A screen showing `SEARCH` is listening for a Beacon.
   An SF shown under `ACKED DATA SF` belongs to the last confirmed packet.
   Read `ADR_TIMING` with `status` before and after the run.

## Save the results

Close monitors before recording. Use the observed ports and a new
directory for each run. Use `--board COMx` to record just the Gateway.
Opening a serial session may reset a board. Check boot boundaries before
joining runs.

```powershell
$python = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$run = Join-Path $env:LOCALAPPDATA 'Adaptive-LoRa/captures/RUN-YYYYMMDD-HHMMSS'
Get-FileHash .pio/build/heltec_wifi_lora_32_V3_adaptive/firmware.bin
& $python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out $run
```

Use the [run record](../../logs/test/RUN_RECORD_TEMPLATE.md). Save the
image hash, board aliases, slots and raw files. Keep raw logs, device
identifiers and private payloads outside Git. Mark Client ACK receipt
as unknown when only a Gateway log is available. For physical timing,
measure DIO1, NSS, BUSY and SCL with high-impedance probes and track
clock drift. Match the test route, load and radio settings before
comparing runs.
