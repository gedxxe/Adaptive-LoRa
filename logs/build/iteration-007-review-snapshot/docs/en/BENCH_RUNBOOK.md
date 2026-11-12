# M0–M2 bench runbook

This is an unexecuted procedure for two physical units. Read HARDWARE.md first. Candidate software success does not close hardware/RF acceptance.

Current inventory correction: two radio modules, one MCU, second MCU not purchased, Moxon not fabricated. The pair is not assembled. This runbook is deferred; current work runs host software tests only.

## Before power or upload

1. Photograph both Super Mini boards and Ra-01SCH markings, revision, pad labels and numbering orientation. Verify GPIO4/5/6/7 SPI, GPIO1 RESET, GPIO0 DIO1 and GPIO10 BUSY by continuity against the proposed map. Do not use the map as measured evidence.
2. Verify flash size and USB path. The candidate enables native USB CDC on GPIO18/19. For a USB-UART board, remove `ARDUINO_USB_MODE` / `ARDUINO_USB_CDC_ON_BOOT` flags deliberately and record/rebuild. GPIO20/21 are UART0; avoid strapping pins 2/8/9 for radio control.
3. Verify regulated radio 3.3 V, shared ground, local decoupling and TX transient voltage within 2.7–3.6 V. Check regulator margin. Connect a verified antenna/load and 50-ohm feed before enabling a valid role. Leave TXEN/RXEN unconnected unless exact breakout instructions require otherwise.
4. Record permitted test frequency/channel, conducted power, EIRP including Moxon gain/feed loss, access requirements and site/equipment authorization. The source's 915.0 MHz is provisional, not a compliance finding.
5. Verify module oscillator/regulator/RF-switch behavior: candidate uses XTAL (DIO3 voltage 0), LDO mode and RadioLib default DIO2 RF-switch handling. A selected LLCC68 driver and successful library chip check are not independent silicon identification; RadioLib can internally accept an SX1261 identification string for LLCC68.

## Build and flash the same image

```powershell
pio run -e esp32c3_supermini
pio test -e native
pio run -e esp32c3_supermini -t upload --upload-port COM_GATEWAY
pio run -e esp32c3_supermini -t upload --upload-port COM_CLIENT
```

Replace port placeholders. If `pio` is not on PATH, use `& <PIO_PYTHON> -m platformio` on this workstation. Normal package-cache writes and host-tool startup may require execution outside the restricted sandbox. If redirected package listings fail in cp1252, set `PYTHONUTF8=1` for that command/process.

The current workstation's `espressif32` install is the pioarduino fork 55.03.311, not the official platform's usual 6.x distribution. Record the resolved package log; another workstation's unpinned `espressif32` may resolve differently. Do not silently claim that a compile on one toolchain verifies another.

## Provision both units at 115200 baud

On an unconfigured board, `status` must show PROVISIONING, UNSET, invalid/missing config and disabled radio. Defaults shown with `scope=inactive_defaults` are staging suggestions, not loaded valid values. No LLCC68 initialization or application TX is started in this mode.

Gateway:

```text
config set role gateway
config set network-id 1
config set node-id 1
config set gateway-id 1
config set hello-interval-ms 5000
config show
config save
status
reboot
status
```

Client: use the same sequence with `role client` and `node-id 2`. Role edits preserve all IDs. `config show` reports staged values; `status` reports the loaded boot configuration. Save must report `verified_reboot_required`; role/IDs/interval remain unchanged in the running app until controlled reboot. The gateway's node must equal its gateway ID; the client's must differ. Network ID 65535 is valid; node/gateway IDs 0 and 65535 are reserved.

Negative provisioning checks (capture all output):

- Refuse save before selecting a role; refuse gateway node 2/gateway 1 or client node 1/gateway 1. Restore the correct staging values afterward.
- Refuse numeric signs, overflow, trailing junk, node 0/65535 and an overlong command line. A following `status` must still work.
- Save, power-cycle and verify schema 1, role and IDs persist. Edit a role during a transaction, save, verify the old app still runs, then `reboot`; new TX requests stop and active TX completes or times out before restart.
- Native tests cover missing/partial/corrupt config representations and schema validation. Physical corrupt/unsupported NVS tests require a controlled NVS fixture on a bench unit: one truncated blob, one damaged checksum, one valid-integrity schema/role mismatch. Back up the unit's configuration, load the fixture using a dedicated NVS test harness, reboot the production image and verify no TX. Such fixture injection is NOT implemented as a production serial command and has NOT been performed. Restore valid configuration after each case.
- Do not use packet logging in NVS or mass-flash erasure as an undocumented test step.

## Bring up and capture the link

1. Initialize gateway first, then client. Verify `RADIO_INIT code=0`, loaded role/IDs and no brownout/watchdog cycle on both. Observe BUSY and DIO1 with suitable instrumentation and record reset behavior. Initialization failure requires inspecting wiring/power and then a controlled reboot; the firmware does not silently substitute another radio class.
2. Use fixed SF7/BW125/CR4/5, explicit header, CRC, preamble 8, private sync 0x12, 10 dBm. SF9 control is reserved and unused in M2. Never choose SF10–SF12 at BW125.
3. Close serial monitors before dual-port capture. The helper uses pyserial bundled with PlatformIO, sends only `status`, and writes bounded chunks to new files. Port opening may toggle modem lines and reset some boards; record this. Capture defaults to 1200 seconds, enough for 200 attempts at the default interval if the link is running; verify actual counts rather than assuming them.

```powershell
& <PIO_PYTHON> tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 1200 --out logs/test/RUN-YYYYMMDD-HHMM
```

4. Copy `logs/test/RUN_RECORD_TEMPLATE.md` into the run directory as `RUN_RECORD.md` and fill measured details. Raw bytes go to `gateway.log` and `client.log`; `capture.json` records timing, ports and capture errors. Stop capture before reopening a monitor and checking final `status`/`log_dropped`; add that output to the run record.
5. Count client `TX_START result=hello`, `TX_DONE result=hello_sent`, `ACK_RX result=accepted`, `ACK_TIMEOUT` and `HELLO_FAILED` separately. `LINK_COUNTS` is cumulative since client startup. Gateway `HELLO_RX` counts new identities, `DUPLICATE` re-ACKs the same identity, `ACK_TX` means start and `TX_DONE result=ack_sent` means completed TX. Header sequences belong to each sender; ACK_TX also logs the echoed HELLO identity.
6. Match network/source/boot/sequence. Record at least 200 actual attempts, gateway unique receives, completed ACK TX, accepted ACKs, timeouts, errors, RSSI/SNR distributions and `transaction_us`. Report denominators explicitly and reject incomplete captures/log drops as acceptance evidence. The short-range engineering target is >=99% accepted ACK transactions; no field-capacity claim follows.
7. Turn off gateway reception and observe client ACK timeouts with no retransmission scheduler. Restore gateway and verify later new HELLOs succeed. Mismatched ACK source/boot/sequence, malformed frames and duplicate identities are covered in native tests; actual RF injection remains a separate recorded test fixture. Do not label native tests as RF injection.

Radio errors expose available RadioLib codes. `CRC_ERROR` reflects exposed CRC/header-error IRQs, not a calibrated total CRC-failure counter. Unknown metrics use N/A. RX start recovery is bounded to three consecutive failures with one-second spacing; persistent fault requires inspection/reboot. TX has a two-second software deadline; ACK wait is one second after TX completion. New HELLOs wait the configured interval after result, with a fresh nonzero session before sequence reuse. No M2 retransmission, TDMA or ADR is implemented.

## Evidence gates and next milestones

- M0: close board/module/pin, power, antenna, USB and RF authorization checks; preserve exact package/compiler versions.
- M1: both units initialize, BUSY/DIO1/reset verified, configuration persists, invalid configuration never starts radio TX, stable serial/no resets.
- M2: native and firmware validation plus actual HELLO/ACK/timeout/duplicate/negative-identity evidence and the >=200-attempt run. Keep status IN PROGRESS until all evidence exists.
- M3–M8: implementation sequence and acceptance criteria are in MILESTONE_TASKS.md and TEST_PLAN.md. Begin M3 only after M2's hardware gates; measure switching, IRQ timing and drift before choosing guards. No later-milestone source is included here.
