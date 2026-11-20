# M0–M2 Heltec bench runbook

Use this procedure for the two Heltec WiFi LoRa 32 V3.4 boards and stock
monopoles. The owner confirms the V3.2 wiring and a 902–928 MHz procurement
variant. The V3.2 schematic remains the electrical reference. Record direct
observations by board alias. See [HARDWARE.md](HARDWARE.md).

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

## 3. Check peripherals and flash the OLED candidate

Compare the V3.2 reference with the delivered board. Record front/back marks,
physical RF SKU, connector and monopole markings, flash size and supply. Check
OLED SDA/SCL/reset, Vext polarity and settling, and the expected SSD1306 `0x3C`
interface. Record any measurements and open questions. Proceed with the OLED
image only after these checks and the private backup/layout review. Check the
actual image hash and retain it in the run record. Use the same image on both
boards. Keep serial monitors closed during each manual upload.

```powershell
pio run -e heltec_wifi_lora_32_V3_oled
Get-FileHash -Algorithm SHA256 .pio/build/heltec_wifi_lora_32_V3_oled/firmware.bin
pio run -e heltec_wifi_lora_32_V3_oled -t upload --upload-port COM_BOARD
pio device monitor --port COM_BOARD --baud 115200
```

Replace `COM_BOARD` separately for each board. After upload, record `BOOT`,
`status` and the OLED startup result. With missing/invalid config, expect
provisioning and radio disabled. A valid saved role can initialize and transmit.
Do not erase NVS to force provisioning. The default OLED-off environment
`heltec_wifi_lora_32_V3` is available for a later matched comparison.

## 4. Provision each board separately

At 115200 baud run `status` first and record loaded values. Apply the following
blocks to their respective ports. Check staged values with `config show`,
require `CONFIG_SAVED`, then reboot under operator control and use `status` to
confirm the loaded role, network, node, gateway, schema 1 and `3000` ms interval.
A failed save is a stop point. Keep RF terminated while provisioning.

Gateway:

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
reboot
status
```

Client:

```text
config set role client
config set network-id 1
config set node-id 2
config set gateway-id 1
config set hello-interval-ms 3000
config show
config save
reboot
status
```

Both intervals are deliberate. The Client waits 3 s after each transaction.
The Gateway uses its interval to interpret freshness. With these settings, the
OLED shows `WAIT` before the first qualifying reception and `ONLINE` for under
7 s after the latest valid HELLO at Gateway or accepted ACK at Client. It then
shows `STALE`. `ONLINE` describes receive recency. It provides no guarantee
that the peer is still reachable. Check `RADIO_INIT`, `OLED_INIT`, serial status
and reset boundaries on each board before joining the pair.

For M0–M2, confirm `profile=m2` and `protocol=1`. Before starting the
22 dBm candidate, follow [HARDWARE.md](HARDWARE.md) to verify the load,
supply margin and authorized RF conditions. A separate v3 test follows the
[startup guide](STARTUP_AND_PROVISIONING.md) after the baseline capture
and uses the multi-client image on both nodes.

## 5. Capture the two-board smoke test

Radiated testing requires an authorized RF setup for the fixed provisional
915.0 MHz profile. Start with Gateway, then Client, approximately 3 m apart
with both stock monopoles vertical. Keep the approved setup and PHY fixed.
Close interactive monitors. Use the observed ports and a new run directory.

```powershell
python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 120 --out logs/test/RUN-YYYYMMDD-HHMMSS-smoke
```

Use PlatformIO Python. Copy the [run-record template](../../logs/test/RUN_RECORD_TEMPLATE.md)
and record board aliases, image hash, provisioning, supplies, antenna details,
geometry, OLED screens and `OLED_STATUS` timing. Preserve the raw logs, chunk
indexes and capture metadata. Require complete capture, stable boot boundaries
and no log drops for the chosen window. Check a completed Client HELLO,
Gateway reception, Gateway ACK start and completion, and Client acceptance of
the echoed identity. Check both RSSI/SNR directions. Save any failure and stop
before range testing if basic communication or capture is unreliable.

## 6. Compare OLED builds, then extend range

Repeat under the same geometry, supply, PHY and interval with OLED on and off.
Compare PDR, ACK success, transaction durations, resets and log drops. In the
OLED run, request `status` at the beginning and end to capture maximum write
and refresh I/O spans. For direct IRQ timing, use high-impedance probes on DIO1,
SCL and radio NSS. Compare DIO1-to-first-radio-SPI service and ACK TX turnaround
for HELLOs overlapping OLED writes. A write API timing counter does not measure
that latency. Keep a separate run ID for each image and bench condition.

Progress through 10, 25, 50, 100, 200 and 400 m only after the smoke test works.
Keep antenna type, height, polarization, PHY and duration fixed for comparable
conditions. Short survey windows are exploratory. Acceptance requires at least
200 fully observed HELLO TX completions in each selected condition. Plan 3,600 s
at the 3 s interval, with extra time if needed. Check packet count directly.
Separate hosts can capture with `--board COM_GATEWAY` and `--board COM_CLIENT`
and matching run IDs with distinct suffixes. Follow
[LOGGING_GUIDE.md](LOGGING_GUIDE.md) for cohort joins, PDR and ACK success,
directional RSSI/SNR, transaction duration, reset boundaries and exclusions.
Missing log lines leave the RF outcome unknown. Screenshots are supplementary.

M0–M2 acceptance still needs board and electrical results, physical NVS
save/reload, a closed-cohort normal link and separate controlled negative and
recovery evidence. ERR-021 remains open. Timing results and M2 evidence precede
closing the scheduled research acceptance checks.

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
