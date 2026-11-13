# M0–M2 bench runbook

This is an unexecuted procedure for one MCU-only check and the future two-radio pair. Read HARDWARE.md and PHYSICAL_ACCEPTANCE_READINESS.md first. Candidate software success does not close hardware/RF acceptance. Use the MCU-only branch below before applying the two-device sections.

Current inventory correction: two radio modules, one MCU, second MCU not purchased, Moxon not fabricated. The pair is not assembled. Two-device execution is deferred; current work prepares procedures and validates software only. No physical step below has been executed.

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

Replace port placeholders. If `pio` is not on PATH, use `python -m platformio` on this workstation. Normal package-cache writes and host-tool startup may require execution outside the restricted sandbox. If redirected package listings fail in cp1252, set `PYTHONUTF8=1` for that command/process.

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
3. Close serial monitors before dual-port capture. The helper uses pyserial bundled with PlatformIO, sends only `status` at open and at normal completion (one-second tail), and writes bounded chunks plus offset/time journals to new files. DTR/RTS are set false before open; OS/board glitches can still reset a board. Record actual reset observations; do not assume opening is reset-free. Capture defaults to 1200 seconds, enough for 200 attempts at the default interval if the link is running; verify actual counts rather than assuming them.

```powershell
python tools/capture_bench.py --gateway COM_GATEWAY --client COM_CLIENT --seconds 1200 --out logs/test/RUN-YYYYMMDD-HHMM
```

4. Copy `logs/test/RUN_RECORD_TEMPLATE.md` into the run directory as `RUN_RECORD.md` and fill measured details. Raw bytes go to `gateway.log` and `client.log`; per-stream `.chunks.jsonl` records observation times/offsets, and `capture.json` records schema2/state/ports/byte counts/hash/tails/errors. Require final status from the same open connection after the analysis cohort. Reopening a monitor can reset and clear counters; a new post-reset zero is not an end-of-run drop measurement. Default final status/tail is best effort: missing final status blocks the evidence gate.
5. Count client `TX_START result=hello`, `TX_DONE result=hello_sent`, `ACK_RX result=accepted`, `ACK_TIMEOUT` and `HELLO_FAILED` separately. `LINK_COUNTS` is cumulative since client startup. Gateway `HELLO_RX` counts new identities, `DUPLICATE` re-ACKs the same identity, `ACK_TX` means start and `TX_DONE result=ack_sent` means completed TX. Header sequences belong to each sender; ACK_TX also logs the echoed HELLO identity.
6. Match network/source/boot/sequence. Select a fully observed cohort with at least200 completed physical HELLO transmissions; separately report API attempts A (including start failures), TX completions T, gateway unique R, ACK starts/completions and accepted C. Use the LOGGING_GUIDE.md CSV specification: PDR=|R intersect T|/|T| and transaction success=|C|/|A|; reconcile counters and boundary exclusions, never infer200 from duration. Incomplete/ambiguous records or nonzero relevant log drops cannot establish acceptance. The existing short-range engineering target is >=99% accepted transactions on that stated denominator; it is provisional, not a field-capacity/reliability guarantee.
7. Turn off gateway reception and observe client ACK timeouts with no retransmission scheduler. Restore gateway and verify later new HELLOs succeed. Mismatched ACK source/boot/sequence, malformed frames and duplicate identities are covered in native tests; actual RF injection remains a separate recorded test fixture. Do not label native tests as RF injection.

Radio errors expose available RadioLib codes. `CRC_ERROR` reflects exposed CRC/header-error IRQs, not a calibrated total CRC-failure counter. Unknown metrics use N/A. RX start recovery is bounded to three consecutive failures with one-second spacing; persistent fault requires inspection/reboot. TX has a two-second software deadline; ACK wait is one second after TX completion. New HELLOs wait the configured interval after result, with a fresh nonzero session before sequence reuse. No M2 retransmission, TDMA or ADR is implemented.

## Evidence gates and next milestones

- M0: close board/module/pin, power, antenna, USB and RF authorization checks; preserve exact package/compiler versions.
- M1: both units initialize, BUSY/DIO1/reset verified, configuration persists, invalid configuration never starts radio TX, stable serial/no resets.
- M2: native and firmware validation plus actual HELLO/ACK/timeout/duplicate/negative-identity evidence and the >=200-attempt run. Keep status IN PROGRESS until all evidence exists.
- M3–M8: implementation sequence and acceptance criteria are in MILESTONE_TASKS.md and TEST_PLAN.md. Begin M3 only after M2's hardware gates; measure switching, IRQ timing and drift before choosing guards. No later-milestone source is included here.

## Single-board branch: MCU only, no radio or antenna required

**NOT RUN.** Physically remove/unpower all RA-01SCH modules and external wiring before every step. Do not select CLIENT for this test. Valid GATEWAY config still causes production LLCC68 initialization after reboot; absent radio may return RADIO_FAULT or block inside a dependency. It is not a radio-off build. If serial progress stops, stop the test and use ROM recovery; do not claim persistence/console PASS from a hang. Never connect a radio during this branch.

1. Photograph/label the one board, chip/flash marking, USB connector/regulator, BOOT/RESET controls and actual revision. Record flash-size/USB assumptions, cable and host. For first power, enter identified ROM download mode before connecting USB so existing application code does not run; if board controls are unknown, stop for identification. Powered enumeration here means ROM mode. Compare powered/unpowered USB enumeration using OS tools and `<PIO_PYTHON> -m serial.tools.list_ports -v`; record VID/PID/port without publishing device identifiers unnecessarily. No port in the013 enumeration was found; it does not prove a device cannot enumerate later.
2. Before opening a monitor/uploading, back up the board in ROM download mode with no radio attached. Identify BOOT/RESET behavior from the actual board; manual download entry normally holds BOOT while resetting. NativeUSB and USB-UART can enumerate differently. If secure boot/flash encryption is enabled, ROM/security/read is restricted, or board identity is uncertain, stop this plaintext backup/restore procedure; do not change eFuses, use force, disable security or guess flash size.
3. Example **future operator commands**, not executed in013. Resolve these installed paths on the host; current help is verified for esptool5.3, whose `.py` wrapper gives a nonblocking deprecation notice. `no-reset` does not replace manual ROM entry or guarantee no USB/modem glitch.

```powershell
$PioPython = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$Esptool = Join-Path $env:USERPROFILE '.platformio/packages/tool-esptoolpy/esptool.py'
# Replace PRIVATE_BACKUP_DIR with a private new directory outside publication.
& $PioPython $Esptool --chip esp32c3 --port COM_BOARD --before no-reset --after no-reset flash-id
& $PioPython $Esptool --chip esp32c3 --port COM_BOARD --before no-reset --after no-reset get-security-info
& $PioPython $Esptool --chip esp32c3 --port COM_BOARD --before no-reset --after no-reset read-flash 0 ALL PRIVATE_BACKUP_DIR/full-before-a.bin
& $PioPython $Esptool --chip esp32c3 --port COM_BOARD --before no-reset --after no-reset read-flash 0 ALL PRIVATE_BACKUP_DIR/full-before-b.bin
Get-FileHash PRIVATE_BACKUP_DIR/full-before-a.bin -Algorithm SHA256
Get-FileHash PRIVATE_BACKUP_DIR/full-before-b.bin -Algorithm SHA256
```

4. Require two identical full reads while the application is stopped, correct detected capacity and private SHA/board mapping. Preserve firmware, partition table and complete NVS/other partitions; backups may contain credentials and must not be committed. Decode the actual partition table using the installed `gen_esp32part.py` and compare with the offline project table. Current build expects table0x8000, NVS0x9000/0x5000, app0x10000, but these are **not measured board offsets**. If layout/encryption differs, stop for a reviewed recovery/migration plan. Hash the baseline NVS slice as well.
5. Build/test and review the actual upload images/offsets before running the existing PlatformIO upload command for COM_BOARD. This resolved platform writes separate bootloader/partition/boot_app0/app images; never substitute `firmware.factory.bin` at0, mass erase, `--erase-all` or `--force` as a routine upload. Require no image/erased sector overlaps the verified NVS range and a compatible partition layout. Record actual command/exit/uploaded binSHA, then read/hash the NVS range again in ROM before running app if preservation is being tested. Stop on unexpected changes. New-board layout installation is an explicit backed-up setup operation, not a claim of preserving an incompatible old layout.
6. Reset to production image, open115200 and capture using the same connection:

```powershell
& $PioPython tools/capture_bench.py --board COM_BOARD --seconds 180 --out logs/test/RUN-YYYYMMDD-HHMMSS-board --metadata PRIVATE_RUN_METADATA.json
# --no-status instead gives passive input only; it cannot prove final drop counts.
```

   `Serial.begin(115200)` is the firmware setting; nativeUSB baud does not establish a physical UART bitrate. Capture opening/closing may reset; record BOOT/reset_reason and port changes. Do not run a monitor and capture against the same port simultaneously. To issue provisioning commands, finish capture, open an interactive monitor115200, record its transcript/any reset, and use fresh run directories for each phase.
7. If NVS is missing/invalid, require CONFIG_INVALID, PROVISIONING/radio_disabled and `status` modePROVISIONING/radio=disabled, loaded UNSET/inactive defaults, with no RADIO_INIT/TX_START. Existing valid config is **not** missing. A valid role attempts radio initialization before app/serial loop; capture that actual result rather than expecting provisioning automatically.
8. Optional controlled missing-config setup on the designated scratch board: only after steps2-5, verified complete backup/recovery and recorded operator intent. With no radio, enter ROM; verify NVS offset/size from the actual compatible table, then `erase-region <VERIFIED_NVS_OFFSET> <VERIFIED_NVS_SIZE>` with the same chip/port/no-reset options. This deliberately erases **only the backed-up NVS partition**. Do not erase other ranges or call it nondestructive. Reset, verify the missing-config behavior in7, and restore afterward. If no such write is authorized for the board, leave this check NOT RUN; do not overwrite saved config merely to create a test condition.
9. In provisioning mode exercise help/status/config show; stage an invalid Gateway node2/gateway1 config, require config save rejection, and verify loaded state stays UNSET. Try signed/overflow/overlong commands and following status. This checks serial validation, **not corrupt stored-NVS boot rejection**. Corrupt/unsupported blob boot tests remain a separate controlled fixture gate below; no fixture was injected in013.
10. For NVS save/readback, explicitly stage GATEWAY/network1/node1/gateway1 and interval5000, inspect config show, save and require CONFIG_SAVED/verified_reboot_required. Status still shows the originally loaded role until reboot. Send controlled reboot: after stop handling and100ms interval it should restart; on this radio-less MCU, require loaded config fields if initialization returns, and record RADIO_INIT failure/RADIO_FAULT. If it does not return, record BLOCKED and recover via ROM; do not mark it a successful NVS test.
11. Power off/on only with radio absent, preserve each boot transcript and verify schema1/IDs/Gateway role actually load. Confirm no normal protocol TX initiation; absence of a radio means no physical LoRa TX can be measured. This power-cycle observation is distinct from software object recreation. Do not erase/reflash between save and persistence check.
12. Restore original state or retain an explicitly designated bench configuration, with the operator recording which. NVS-only restoration uses the exact saved NVS slice at the verified original range; full-state recovery writes the verified same-board full backup at0 with esptool write-flash `--flash-mode keep --flash-freq keep --flash-size keep`, with radio disconnected and ROM/security preconditions unchanged. Full restoration intentionally writes old configuration too. Re-read and compare restored range/full hash before leaving ROM; failure is not PASS. Do not change encryption/eFuse settings or transfer backups between boards. A subsequent production boot may load the restored valid role and attempt radio init; keep radio absent until the two-device gates are complete.

## Explicit pair acceptance checklist

All entries currently **NOT RUN/BLOCKED**, not software PASS. Use RUN_RECORD_TEMPLATE.md and keep raw evidence per gate.

| Gate | Required evidence | Stop/remaining condition |
|---|---|---|
| M0 identity/wiring | Photos/revisions, pin orientation and power-off continuity for every proposed net, actual USB/flash/board compatibility | No clone schematic/continuity is currently verified |
| M0 supply/RF | Measured3.3V/range/transient/ripple at radio, common ground/decoupling, suitable attached50-ohm feed/antenna/load, RF authorization record | SecondMCU/antenna/instruments/authorization pending; no bare-port TX |
| M1 initialization | Same bin on both, LLCC68 driver init result plus physical markings, SPI/BUSY/DIO1/reset traces and stable supply/serial | Driver label alone is not independent chip identification |
| M1 no unintended TX | Unconfigured boot emits no radio init/TX; configured Gateway listens; deliberately start Client only after all RF gates | Radio is not a provisioning-only build; stored valid role activates automatically |
| M1 recovery | Observed bounded returned-error recovery, no reset/brownout/log loss; disposition of stuck-BUSY progress watchdog/library risk | ERR-021 blocks unattended/stuck-BUSY recovery acceptance; do not force BUSY pins electrically without a reviewed isolated fixture |
| M2 normal link | >=200 completed HELLO cohort, fixed PHY, separate A/T/R/duplicates/ACK starts/completions/C, counters and end status reconciled | Measurement denominators/CSV spec in LOGGING_GUIDE.md; no target inferred from run duration |
| M2 timeout/restore | Gateway reception unavailable produces client timeout, restores to later new-identity success, no automatic packet retry | Keep antennas/loads and legal settings in place; record intervention times |
| M2 duplicate/identity | Controlled replay of identical network/source/boot/sequence/payload produces DUPLICATE/re-ACK without duplicate delivery; wrong ACK echo not success | Requires reviewed replay/injection fixture using one of the two units sequentially; NOT PROVIDED/EXECUTED here. Native cases are separate evidence |
| M1/M2 corrupt config | Backed-up controlled truncated/corrupt/schema-invalid NVS fixture yields provisioning/no TX, then verified restore | Physical fixture NOT PROVIDED/EXECUTED; staging rejection does not close this gate |

M0/M1/M2 overall remain IN PROGRESS until each required gate is evidenced. For any stall/loss of serial progress, stop acquisition as failed, remove MCU and radio power safely, preserve logs and inspect; do not disconnect the RF termination while radio is powered. Normal attended link observations cannot close ERR-021. No M3 begins on the strength of these procedures alone.

SDK preservation caveat: installed Arduino initArduino calls nvs_flash_init **before** FirmwareRuntime. On ESP_ERR_NVS_NO_FREE_PAGES or ESP_ERR_NVS_NEW_VERSION_FOUND it erases the NVS partition and retries (esp32-hal-misc.c:331-337). Therefore even an upload that does not overlap NVS cannot guarantee its preservation on first application boot. Backups precede that boot; record post-boot contents and distinguish SDK formatting from DeviceConfig blob rejection. Do not inject whole-partition corruption on an unbacked board. This policy was source-inspected, not physically exercised.
