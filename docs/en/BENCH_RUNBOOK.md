# Bench testing

Use this guide for first-power backup, USB troubleshooting and RF measurement.
The [main guide](../../README.md#flash-and-configure) has the upload and
role commands. Verify the delivered Heltec WiFi LoRa 32 V3.4 board against
the V3.2 electrical reference and record observations by board alias.
See [hardware details](HARDWARE.md).

## 1. Identify the boards and Windows serial ports

Label the boards Gateway and Client while they are disconnected. Inspect each
RF connector and the supplied antenna or load before connecting USB. A factory
image or saved role could transmit as soon as the board boots. Fit a suitable
50-ohm load for first power, and use stable USB 5 V with adequate current
margin. Keep the load connected through backup, upload and provisioning.

Use a known USB-C **data** cable. A charge-only cable can light the board without
creating any Windows device. Connect one board directly to the PC, without a
hub, and compare [Device Manager](#windows-device-manager) and `pio device list`
before and after connecting. Use the board's manual ROM download procedure for
first power when the factory/NVS image needs to be preserved before app boot.
After identifying that board's port, perform its backup before repeating for the
second board. Record alias, USB VID/PID, device description and COM number.
The build uses the CP2102 USB-to-UART bridge and UART0 at 115200 baud.

### Windows Device Manager

1. In **Ports (COM & LPT)** look for the CP210x USB-to-UART bridge and its COM
   number. Verify the entry disappears when that board is unplugged. Bluetooth
   virtual ports are unrelated. A COM assignment can change after reconnection.
2. If there is no new COM port, check **Universal Serial Bus devices** and
   **Other devices** for the new entry or a warning icon. Under **Properties →
   Details → Hardware Ids**, record the observed VID/PID and description.
3. If the bridge enumerates without a COM port or reports a missing driver,
   install the **Silicon Labs CP210x Universal Windows Driver** for the host
   from the [official VCP driver page](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers).
   Reconnect and repeat the comparison. If nothing enumerates, try another
   known data cable and a direct USB port before diagnosing the driver.
4. Confirm the same entry with `pio device list`. If no board-specific COM
   port is visible, stop before upload or capture. Use the observed port
   number in every command. Opening a serial session can toggle DTR/RTS and
   reset the board, so defer monitor and capture until preservation is complete.

## 2. Preserve each board and verify flash layout

Enter ROM download mode manually using the board's PRG/BOOT and RESET controls.
At first power, hold PRG/BOOT while connecting power, then release it after the
boot strap has been sampled. If already powered, hold PRG/BOOT while briefly
pressing RESET, then release PRG/BOOT. Confirm the board's actual button labels
and keep RF terminated. These are operator steps.

Use the installed PlatformIO Python and esptool 5.x. The following PowerShell
examples use the standard PlatformIO install paths. Substitute the observed COM
number and a separate private directory for each board. None of the backup commands
requests an automatic reset or writes flash.

```powershell
$pioPython = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$pioEsptool = Join-Path $env:USERPROFILE '.platformio/packages/tool-esptoolpy/esptool.py'
$benchPort = 'COM_BOARD'
$backupDir = 'PRIVATE_DIRECTORY_FOR_THIS_BOARD'
& $pioPython $pioEsptool --chip esp32s3 --port $benchPort --before no-reset --after no-reset --no-stub get-security-info
& $pioPython $pioEsptool --chip esp32s3 --port $benchPort --before no-reset --after no-reset --no-stub flash-id
```

Review the results before proceeding. If the flash is confirmed as 8 MB and
security settings permit an ordinary readable backup with the RAM stub, make two
independent reads using new file names. Stop on any nonzero exit, unreadable
security state, unexpected capacity or restriction. Preserve the transcripts
privately. Do not use force, erase flash or change eFuses to obtain a backup.

```powershell
& $pioPython $pioEsptool --chip esp32s3 --port $benchPort --before no-reset --after no-reset read-flash 0x0 0x800000 (Join-Path $backupDir 'flash-a.bin')
& $pioPython $pioEsptool --chip esp32s3 --port $benchPort --before no-reset --after no-reset read-flash 0x0 0x800000 (Join-Path $backupDir 'flash-b.bin')
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $backupDir 'flash-a.bin'), (Join-Path $backupDir 'flash-b.bin')
```

Require two 8,388,608-byte files with matching hashes. Verify the actual
partition table from the backup and record NVS offset/size, security flags and
a same-board recovery plan. Save the NVS bytes as part of the private backup.
Compare the board's layout with the build's bootloader, partition table and app
write ranges. The default build's NVS range starts at `0x9000` and is `0x5000`
bytes, and `app0` starts at `0x10000`. Those addresses describe the build.
Stop if the actual layout or safe restore path is uncertain. Keep backups out of
Git and restore only to the same board. ERR-023 allows the SDK to format NVS
before the project runtime starts, even if the upload doesn't overlap NVS.

## 3. Verify the v3 pair

Before upload, compare the board marks, RF SKU, connector, flash size and
power supply with the [hardware reference](HARDWARE.md). Check the display
pins, Vext polarity and settling, and the expected SSD1306 address `0x3C`.
Verify that the antenna or load and supply can support the requested `22 dBm`
profile in the authorized setup. Record unresolved checks with the run.

After backup and layout checks, use the [main guide](../../README.md#flash-and-configure)
to upload and configure Gateway 1 and Client 2. Record the image hash,
startup output and serial `status` for each board.

```powershell
Get-FileHash -Algorithm SHA256 .pio/build/heltec_wifi_lora_32_V3_multiclient/firmware.bin
```

With authorized RF settings, run the pair at short range with fixed antenna
orientation and supply. Close monitors and record both ports together.
Substitute the observed ports and use a new output directory for each run.
Opening a serial port can reset the board. Confirm boot boundaries in the logs.
The example writes raw captures outside the Git workspace.

```powershell
$pioPython = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$captureDir = Join-Path $env:LOCALAPPDATA 'Adaptive-LoRa/captures/RUN-YYYYMMDD-HHMMSS-smoke'
& $pioPython tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out $captureDir
```

Match `V3_DATA_TX`, Gateway `APP_PAYLOAD`, and the Client's
`V3_PAYLOAD_RESULT` by client identity and frame. Check accepted results,
radio errors, missed launches and dropped serial lines. Test a Gateway
power cycle in a separate run. The Client should enter beacon search,
then accept a new valid schedule and resume DATA exchange.
Use the [display diagnostics](DISPLAY_DIAGNOSTICS.md) to interpret freshness.

## 4. Extend the test to eight Clients

Add Clients in stages of 2, 4, and 8 and keep the powers, antennas, payload
lengths and geometry fixed for comparable runs. Repeat each run at the
declared load. Use one `--board COMx` capture per extra board into a
separate output directory and record a common run ID with distinct board
aliases. Close monitors on those ports before capture.

- Preserve the [run record](../../logs/test/RUN_RECORD_TEMPLATE.md),
  firmware hash, provisioning, antenna geometry, supply and raw captures.
- Count the actual attempts and received packets. Record first-pass delivery,
  retries, ACK agreement, both directions of RSSI/SNR, missed deadlines and
  resynchronization time. Mark gaps or serial drops as incomplete evidence.
  The [logging guide](LOGGING_GUIDE.md#protocol-v3-capture) defines the joins.
- Measure DIO1, NSS and SCL with high-impedance probes. Capture DIO1-to-service
  delay, TX preparation and RX re-arm. Measure oscillator drift over multiple
  superframes before changing the 50 ms guard.
- Run the [offline ADR evaluator](SCHEDULED_LINK.md#link-adaptation-experiments)
  on Gateway observations and compare them with Client logs before planning
  adaptive PHY tests.

## Legacy M0–M2 acceptance

Use `heltec_wifi_lora_32_V3` for an explicit v1 HELLO/ACK comparison.
Confirm `profile=m2` and `protocol=1`. The M2 record needs physical
board and electrical checks, same-board NVS save/reload, at least 200
fully observed HELLO transmissions per selected condition and separate
controlled loss/recovery runs. These measurements are separate from v3.
For the original M2 bench conditions, see the [test plan](TEST_PLAN.md).

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
