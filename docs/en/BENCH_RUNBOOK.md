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

## Adaptive v4 walk test

Before uploading, check board markings, antenna and connector, flash size
and supply against the [hardware guide](HARDWARE.md). Verify the display
wiring and Vext polarity on new boards. Confirm that the supply and antenna
or load are suitable for the requested 22 dBm profile in the authorized setup.
Follow the [README](../../README.md#flash-and-configure) to upload and set
each role. Use Gateway 1, roaming Client 2 and stationary Clients 3–5 on
network 1. Check the image and serial `status` on every board.

```powershell
Get-FileHash -Algorithm SHA256 .pio/build/heltec_wifi_lora_32_V3_adaptive/firmware.bin
```

1. Run one pair near each other for four complete frames, about a minute.
   Match `APP_PAYLOAD` to the Client's `V3_PAYLOAD_RESULT,result=accepted`,
   then check `ACKED DATA SF7` on the Client. Power-cycle the Gateway in
   a separate test and check that the Client resumes.
2. Leave the Gateway and Clients 3–5 in place. Capture continuously while
   Client 2 spends a minute nearby, walks the route, and stops for about
   two minutes at the destination. Record departure and arrival times.
   Keep antenna orientation consistent.
3. Compare delivery before and after retries, SF changes and gaps for
   **each** Client. Check `deadline_miss`, resets and log drops. Run
   `status` before and after to record `ADR_TIMING`. Interpret events
   using the [serial logging guide](LOGGING_GUIDE.md).

## Capture serial logs

Close monitors before recording and use a new directory outside the Git
workspace. Replace the two ports in this short pair example. For a roaming
run, use `--board COMx` on the stationary Gateway and capture the roaming
Client on a second laptop if available. Use the same run ID in both places.
Opening a serial port can reset a board. Check boot boundaries in the logs.

```powershell
$pioPython = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$captureDir = Join-Path $env:LOCALAPPDATA 'Adaptive-LoRa/captures/RUN-YYYYMMDD-HHMMSS-smoke'
& $pioPython tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out $captureDir
```

Use the [run record](../../logs/test/RUN_RECORD_TEMPLATE.md) for the image
hash, board aliases, supply, antenna geometry and raw capture. If only the
Gateway log is available, mark the Client's ACK or control losses as unknown.
Avoid publishing raw logs, device identifiers or private payloads.
For direct timing measurements, use high-impedance probes on DIO1, NSS,
BUSY and SCL to observe IRQ service, RX rearm, display access and drift
before changing the guard.

For a fixed-SF comparison, use the same boards and route with the build
listed in [Versioning](VERSIONING.md). Hold payload, power and antenna
settings constant and record the different control SF and frame period.
Earlier HELLO/ACK procedures and the 200-transaction minimum are described
in the [test plan](TEST_PLAN.md).

<details>
<summary>Controlled BUSY fault procedure</summary>

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

</details>
