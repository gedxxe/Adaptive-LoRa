# Heltec V3.2 hardware migration audit

Iteration 014, 2026-11-17 (client date). Baseline: `075b8faf64ca8377cc2adc396429aa6943b47a27`, including PR #3. The attachment initially requested an audit only. The user then explicitly instructed: “Implement migration now; preserve engineering history.” This report therefore records the hardware audit and the implemented migration. The direct request to push and merge also supersedes the attachment's no-PR clause.

## Scope and evidence

The only active firmware target is **Heltec WiFi LoRa 32 V3.2 HF**, ESP32-S3FN8 with SX1262, using `heltec_wifi_lora_32_V3`. The procurement choice is fixed; delivery, silkscreen, silicon stepping and individual RF performance have not been inspected. A seller listing or board manifest does not establish those physical facts.

The original source, tests and logs remain identifiable in Git. Previous BUILD_MEMORY, ERROR_LOG, ADR and completed iteration records keep their original hardware descriptions. Active code replaces the old adapter rather than maintaining two radio implementations. Firmware remains `0.2.0-alpha.1`, protocol 1 and NVS schema 1. Scheduling, bitmap ACK, selective retry, ADR, OLED, battery monitoring and application integrations are outside this change.

The exhaustive pre-edit reference list is [iteration-014-dependency-inventory.json](../../logs/build/iteration-014-dependency-inventory.json). It classifies references as A production, B current documentation, C tests/tools, D historical evidence or E obsolete material. Numerical pin declarations and SPI/serial entry points are included explicitly. Historical mentions are evidence to retain, not active build dependencies. The final changed-file/input review records what was actually changed.

## Source register

Manufacturer documents were downloaded, fingerprinted and inspected locally. Schematics for all three revisions were rendered; the V3 sheet is image-only. Original PDFs and vendor assets stay in the ignored research cache and are not redistributed. [Source fingerprints](../../logs/build/iteration-014-hardware-sources.json) identify the exact bytes.

| Source | Use |
|---|---|
| [Heltec product](https://heltec.org/project/wifi-lora-32-v3/) | Product family, HF range, bridge, memory and nominal RF specifications |
| [V3.2 datasheet](https://s.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA_V3.2.pdf) | Pin/resource, supply and procurement specifications; internal revision/date labels are inconsistent, so file identity and schematic revision matter |
| [V3 schematic](https://s.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA(F)_V3_Schematic_Diagram.pdf), [V3.1](https://s.heltec.cn/download/WiFi_LoRa_32_V3/HTIT-WB32LA(F)_V3.1_Schematic_Diagram.pdf), [V3.2](https://s.heltec.cn/download/WiFi_LoRa_32_V3/WiFi_LoRa_32_V3.2_Schematic_Diagram.pdf) | Nets, populated options, regulator, oscillator, RF switch and connector |
| [Heltec update log](https://docs.heltec.org/en/node/esp32/wifi_lora_32/hardware_update_log.html) | Revision changes; this legacy site points to the newer WiKi |
| [HeltecWiKi](https://github.com/HelTecAutomation/HeltecWiKi/tree/07d6ea4725d07fad98a9b556f91d34a653bd2cb4) | Current documentation repository inspected separately from other Heltec products |
| [Vendor SX126x code](https://github.com/HelTecAutomation/Heltec_ESP32/blob/6ab5cf916e01b0930eaa2245323c728f849705c4/src/driver/sx126x.c) | Uses DIO3 TCXO control at 1.8 V; no vendor library added to firmware |
| [Vendor factory test](https://github.com/HelTecAutomation/Heltec_ESP32/blob/6ab5cf916e01b0930eaa2245323c728f849705c4/examples/Factory_Test/WiFi_LoRa_32_V3/WiFi_LoRa_32_V3_FactoryTest_V1/WiFi_LoRa_32_V3_FactoryTest_V1.ino) | Vext polarity cross-check; display delay is not a measured radio startup requirement |
| [PlatformIO board](https://docs.platformio.org/en/latest/boards/espressif32/heltec_wifi_lora_32_V3.html) | Board ID and upload support, cross-checked with installed manifest/variant |
| [ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf), [IDF guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/) | MCU memory, strapping, native USB and platform services; current “stable” guide resolves to a newer SDK, so installed 5.5.5 sources govern this build |
| [RadioLib 7.8.1](https://github.com/jgromes/RadioLib/tree/7.8.1/src/modules/SX126x) | Exact driver API and inherited implementation; latest development code is not the compatibility reference |

## Hardware and revision comparison

| Property | Previous implementation | Selected V3.2 HF target |
|---|---|---|
| MCU and CPU | ESP32-C3 Super Mini, single RISC-V core | ESP32-S3FN8, dual Xtensa LX7, 240 MHz board setting |
| Radio | Separate RA-01SCH/LLCC68 | Onboard SX1262, one radio |
| Flash / SRAM | Generic 4 MB target / 400 KB chip SRAM; actual clone unconfirmed | 8 MB SiP flash / 512 KB chip SRAM; no PSRAM assumed |
| PlatformIO RAM accounting | 327,680 bytes | 327,680 bytes; this is a build budget, not all chip SRAM or measured heap |
| SPI/control | Provisional hand wiring | Schematic-defined GPIO8–14 |
| Radio oscillator / regulator | XTAL argument 0 V / LDO selection | DIO3-powered 32 MHz TCXO, 1.8 V / populated DC-DC network |
| Serial | Native CDC configuration | CP2102 and UART0, 115200 baud |
| External peripherals | No onboard display assumption | OLED, Vext, charge and battery-divider circuitry reserved but unused |
| RF feed | Separate antenna connection | IPEX 1.0, HF matching; 915 MHz design baseline retained |

| Revision | Common resources | Differences that affect migration |
|---|---|---|
| V3 / initial V3.0 | S3FN8, 8 MB flash, 512 KB SRAM, no assumed PSRAM, CP2102, SX1262/TCXO, OLED and same radio GPIO group | Earlier supply/ideal-diode design, MOSFET Vext and TP4054 charger. Do not transfer its power circuit to V3.2 |
| V3.1 | Same radio interface and processor family | Update log removes the ideal-diode section, separates 2.4 GHz RF/crystal placement and adds C-to-C USB support; still earlier charger/Vext/divider design |
| V3.2 | Same radio pins, CP2102, display, flash and SRAM specifications | U5 Vext LDO, U3 LGS4056HDA, revised GPIO37-controlled battery divider and layout; supplied schematic shows 2 A fuse versus earlier 500 mA annotation |

These are schematic/design revisions, not measured silicon revisions. The V3.2 user target must be confirmed on the received PCB. V4 has different power, memory and USB choices and is not a substitute; Wireless Stick Lite and Tracker pinouts are also excluded.

## Complete GPIO allocation

Evidence is the V3.2 schematic, its pinmap and the installed Arduino V3 variant. “Candidate” means a header signal with no function claimed by this firmware; external loads and the delivered board still require inspection. ADC-capable aliases in a variant do not prove availability.

| GPIO | Board function | Firmware use | Availability  Evidence |
|---:|---|---|------|
| 0 | PRG button / boot strap | None | Reserved  Schematic / variant / MCU constraints |
| 1 | ADC_IN battery divider, header | None | Reserved for optional battery measurement  Schematic / variant / MCU constraints |
| 2 | Header | None | Candidate  Schematic / variant / MCU constraints |
| 3 | Header / strapping | None | Avoid external boot-level changes  Schematic / variant / MCU constraints |
| 4 | Header | None | Candidate  Schematic / variant / MCU constraints |
| 5 | Header | None | Candidate  Schematic / variant / MCU constraints |
| 6 | Header | None | Candidate  Schematic / variant / MCU constraints |
| 7 | Header | None | Candidate  Schematic / variant / MCU constraints |
| 8 | Radio NSS | SPI CS | Reserved  Schematic / variant / MCU constraints |
| 9 | Radio SCK | SPI clock | Reserved  Schematic / variant / MCU constraints |
| 10 | Radio MOSI | SPI output | Reserved  Schematic / variant / MCU constraints |
| 11 | Radio MISO | SPI input | Reserved  Schematic / variant / MCU constraints |
| 12 | Radio NRESET | Radio reset | Reserved  Schematic / variant / MCU constraints |
| 13 | Radio BUSY | Radio readiness | Reserved  Schematic / variant / MCU constraints |
| 14 | Radio DIO1 | IRQ latch/level | Reserved  Schematic / variant / MCU constraints |
| 15 | 32 kHz crystal net | None | Reserved, not a free header GPIO  Schematic / variant / MCU constraints |
| 16 | 32 kHz crystal net | None | Reserved, not a free header GPIO  Schematic / variant / MCU constraints |
| 17 | OLED SDA | None | Reserved  Schematic / variant / MCU constraints |
| 18 | OLED SCL | None | Reserved  Schematic / variant / MCU constraints |
| 19 | Native USB D-, header; routing option R3 is NC | None | Requires a separate USB-routing decision  Schematic / variant / MCU constraints |
| 20 | Native USB D+, header; routing option R29 is NC | None | Requires a separate USB-routing decision  Schematic / variant / MCU constraints |
| 21 | OLED reset | None | Reserved  Schematic / variant / MCU constraints |
| 22–25 | No such GPIO numbers on this MCU | None | Unavailable  MCU datasheet |
| 26 | SPICS1/header on schematic | None | Reserve with flash bus; do not allocate without package-specific review  Schematic / variant / MCU constraints |
| 27–32 | SPI0/1 / embedded-flash signals | None | Reserved, not general header resources  Schematic / variant / MCU constraints |
| 33 | Header / alternate octal-memory function | None | Candidate on confirmed FN8 without added memory  Schematic / variant / MCU constraints |
| 34 | Header / alternate octal-memory function | None | Candidate on confirmed FN8 without added memory  Schematic / variant / MCU constraints |
| 35 | Status LED | None | Reserved  Schematic / variant / MCU constraints |
| 36 | Vext_Ctrl | None | Reserved  Schematic / variant / MCU constraints |
| 37 | ADC_Ctrl | None | Reserved  Schematic / variant / MCU constraints |
| 38 | Header | None | Candidate  Schematic / variant / MCU constraints |
| 39 | Header / MTCK | None | Candidate if external JTAG is unused  Schematic / variant / MCU constraints |
| 40 | Header / MTDO | None | Candidate if external JTAG is unused  Schematic / variant / MCU constraints |
| 41 | Header / MTDI; variant default SDA | None | Candidate if JTAG/default I2C is unused  Schematic / variant / MCU constraints |
| 42 | Header / MTMS; variant default SCL | None | Candidate if JTAG/default I2C is unused  Schematic / variant / MCU constraints |
| 43 | U0TXD to CP2102 | Serial TX | Reserved  Schematic / variant / MCU constraints |
| 44 | U0RXD from CP2102 | Serial RX | Reserved  Schematic / variant / MCU constraints |
| 45 | Header / voltage-selection strap | None | Reserved for boot review  Schematic / variant / MCU constraints |
| 46 | Header / boot strap, input-only | None | Reserved  Schematic / variant / MCU constraints |
| 47 | Header | None | Candidate  Schematic / variant / MCU constraints |
| 48 | Header | None | Candidate  Schematic / variant / MCU constraints |

DIO2 and DIO3 are radio-internal board nets, not additional MCU assignments. Instrument radio NSS/SCK/BUSY/DIO1/reset with high-impedance probes; do not drive those nets. A future UART/RS485 allocation could be reviewed among GPIO4–7/38, with a separate DE signal and 3.3 V transceiver. No such interface is implemented. Keep default `Wire` GPIO41/42 distinct from OLED GPIO17/18.

### Vext, battery and supply

V3.2 GPIO36 is **active low**. Q5 and U5 in the schematic, plus the vendor factory example, establish low=on and high=off. The radio and RF-switch bias use VDD_3V3 rather than the OLED's Vext rail, so this firmware does not enable Vext or initialize a display. For a later display feature, set the output latch to the off level before output mode, drive low to enable, wait for a measured/required rail settling interval, then release OLED reset and start I2C. The vendor example waits 100 ms for display startup; that is not a measured radio transition time.

GPIO37 high/pull-up enables the revised V3.2 measurement path through Q3/Q1. Battery monitoring remains absent; do not copy earlier active-low divider logic. The charge IC is LGS4056HDA, not firmware-controlled charging. Verify battery polarity and connector before use.

The datasheet specifies at least 500 mA input capability for USB/5 V and 150 mA for direct 3.3 V. Its example whole-board currents at 868 MHz are about 200/210/230 mA at 14/17/22 dBm; these are vendor conditions, not project measurements or the current at 10 dBm. Use stable regulated 5 V with margin, and measure voltage/current during TX. Do not parallel arbitrary supply outputs, back-power an unpowered board through GPIO, or assume these input figures establish spare external-load capacity.

## RadioLib API and implementation assessment

The adapter contains one `Module` and one `SX1262`. Its constructor receives CS8, DIO1=14, RESET12, BUSY13 and explicit 1 MHz/MSB-first/mode-0 SPI. `begin()` supplies 915 MHz, BW125, SF7, CR4/5, sync0x12, 10 dBm, preamble8, TCXO1.8 V and `useRegulatorLDO=false`. Pinned RadioLib's positional begin overload assigns oscillator/regulator fields before calling its common setup; replacing it with the configuration-struct overload without reviewing those fields would be unsafe.

| Operation | Pinned implementation / migration decision |
|---|---|
| Constructor / reset / identification | SX1262(Module*) selects the expected chip type. Common modSetup/findChip performs reset/read checks. Diagnostic reports selected driver, not an independently exposed chip-ID string |
| TCXO | SX1262.begin -> SX126x.modSetup -> setTCXO(1.8). DIO3 is wired to X2 supply; vendor radio code uses TCXO_CTRL_1_8V. RadioLib default 1.6 V is intentionally not used. Default TCXO delay is 5000 us, not a measured board startup result |
| DIO2 | Common begin checks setDio2AsRfSwitch(true). No duplicate command or external TXEN/RXEN GPIO is needed |
| Regulator | V3.2 L8/C54 network supports DC-DC; false selects setRegulatorDCDC. This differs from the old adapter's LDO argument |
| Current / PA | Common begin sets current limit60 mA, then SX1262 performs PA-clamp fix and output-power selection. Keep 10 dBm; do not copy +22 dBm/OCP changes without supply/board review. Return codes remain checked by RadioLib |
| Header / CRC | Common begin already initializes explicit header and enables two-byte PHY CRC. Driver reassertions are redundant successful register commands, but also established error boundaries; retain them and their fault tests |
| RX / IRQ | Infinite RX timeout0xFFFFFF; pass the full generic RX flags0x272 as flags and DIO1 mask, including header-valid/error, CRC and timeout. Raw SX126x bits match existing compile-time assertions |
| TX | The actual driver still requests standby, clears its event latch and starts transmit; the 2 s software deadline is unchanged |
| Completion / standby | finishTransmit, finishReceive, standby and readData statuses propagate unchanged; actual driver state policy is retained |
| Length / metrics | getPacketLength and RSSI/SNR/time-on-air getters forward through the same interface. Status-less getters still cannot report every SPI error; keep that audit limitation |
| SPI / ISR | SPI operations remain inside the radio adapter/library. ISR only sets the static volatile latch; level sampling preserves the existing fallback for an edge around consumption. Native inputs do not prove physical race behavior |

### ERR-021 persists

`RadioDriver.transmit -> Sx1262Hardware.startTransmit -> PhysicalLayer.startTransmit -> SX126x.stageMode(TX) -> SX126x.launchMode(TX) -> setTx(0)` reaches the same `while(digitalRead(BUSY)) yield()` loop in pinned SX126x.cpp1121–1123. The loop occurs after successful setTx; a permanently high BUSY there can prevent return. Earlier per-transfer timeouts and the driver's 2 s deadline do not preempt it. Changing radio class does not close ERR-021.

Bounded options remain a separately reviewed upstream/library fix, a small project-owned HAL/command path with a proven deadline, or a deliberately configured and fault-tested watchdog/reset strategy. Each needs safe RF termination, serial evidence and controlled BUSY injection. This migration changes none of those policies and does not modify third-party files. Attended bring-up can observe normal operation; unattended recovery acceptance remains open.

## PlatformIO, serial and memory

Installed pioarduino/espressif32 55.03.311 already contains `heltec_wifi_lora_32_V3`; Arduino 3.3.11 and the existing RadioLib 7.8.1 support it. No platform upgrade, vendor library or custom board manifest is needed. The applied environment is:

```ini
[env:heltec_wifi_lora_32_V3]
platform = espressif32
board = heltec_wifi_lora_32_V3
framework = arduino
upload_protocol = esptool
monitor_speed = 115200
lib_deps = jgromes/RadioLib@7.8.1
build_unflags = -std=gnu++11
build_flags =
    -std=gnu++17
    -I src
    -D ARDUINO_USB_CDC_ON_BOOT=0
```

The board manifest itself supplies USB_MODE=1 and core affinity settings. The project removes the copied USB defines and explicitly sets CDC_ON_BOOT=0, which binds Arduino `Serial` to UART0 rather than native CDC. USB-C connects to CP2102; GPIO19/20 remain native-USB alternatives with unpopulated routing resistors. Do not populate jumpers to connect two USB transceivers to one pair. No native USB workflow or driver installation is claimed tested.

UART0 uses TX43/RX44. AvailableForWrite forwards to the UART backend; HardwareSerial.write calls uartWriteBuf under a mutex and may block in uart_write_bytes. The logger's capacity check and 128-byte drain budget remain useful but do not establish a hard execution-time bound. Prior HWCDC-specific measurements/analysis are historical, not evidence for the UART backend.

The installed default_8MB table uses NVS0x9000/0x5000, otadata0xe000/0x2000, app0x10000/0x330000, app1 0x340000/0x330000, spiffs0x670000/0x180000 and coredump0x7f0000/0x10000. The final generated partition table must match before upload. Existing C3 firmware/bootloader/full-flash backups are not S3 restore images. Keep saved-config bytes compatible, but provision each new board deliberately; do not copy an old full-flash image across MCU families. Arduino startup can still format incompatible/full NVS before Runtime (ERR-023), so back up the actual S3 board in ROM before first app boot or intentional writes. Security/encryption/layout restrictions still apply.

The first migrated build reports RAM32,096 (+7,532 against baseline24,564) and flash342,560 (-21,748 against364,308), with an app budget3,342,336 bytes. These are compiler/linker totals on different architectures, not measured free heap, stack headroom or runtime gains. Hardware SRAM512 KB and PlatformIO's320 KB accounting are different quantities. No PSRAM flag is enabled. Dual-core execution does not create extra radio owners or tasks; the cooperative loop remains one coordinator. Reassess loop stack, interrupt latency, RNG entropy and heap fragmentation on the delivered S3. Future Wi-Fi would add contention and memory pressure, but remains disabled.

## Retained and replaced modules

| Module | Action and reason |
|---|---|
| main.cpp | Retain byte-for-byte, 15-line entry point |
| GatewayApp / ClientApp | Retain transitions, identities, retry/timeout and recovery policy |
| FrameCodec / SequenceTracker / DeviceConfig | Retain wire/config/session logic; v1 and schema1 unchanged |
| NvsConfigStore / PreferencesConfigStorage / console | Retain namespace/key/24-byte bytes and staged-until-reboot policy |
| RadioTransport / RadioHardware | Retain interfaces; clarify generic SX126x status-bit comment |
| RadioDriver | Retain state implementation; selected-driver diagnostic text becomes SX1262 |
| FirmwareRuntime | Retain implementation except target-specific entropy comment |
| Esp32Firmware | Construct Sx1262Hardware instead of the removed active adapter |
| Radio adapter | Replace old source/header with sx1262_hardware.h/.cpp; one Module/one SX1262 |
| board_pins / radio_config / platformio.ini | Apply V3.2 pins, TCXO/DC-DC and sole S3 environment/UART0 selection |
| Diagnostics / capture utility | Retain formatting, bounded capture and status-only commands |
| RNG reproduction helper | Update only the excluded hardware source filename so its historical baseline probe still builds |
| Current documentation / AGENTS | Apply current target and physical procedure; preserve old iteration evidence |

## Test migration matrix

| Suite | Original cases | Migration treatment |
|---|---:|---|
| App integration | 38 | Retain, production apps with simulated transport/time |
| Configuration store | 56 | Retain actual store/golden bytes; not physical flash durability |
| Runtime lifecycle | 47 | Retain actual coordinator; simulated SDK/reset/entropy |
| Frame codec | 18 | Retain exact bytes and invalid-frame boundaries |
| Provisioning console | 10 | Retain parser/budgets |
| Radio driver | 71 | Retain all policy/fault cases; one expected driver label changes |
| Role/config | 13 | Retain schema and ID rules |
| Runtime reliability | 24 | Retain boot-ID and delayed-operation cases |
| Sequence tracker | 7 | Retain replay/session/wrap behavior |
| Serial logger | 6 | Retain formatter/queue tests; SDK UART timing remains physical/source review |
| SX1262 adapter | 12 new | Compile actual production adapter with bounded call-recording SPI/RadioLib headers; assert board args, TCXO/DC-DC, error propagation, RX mask, ISR latch, byte forwarding and driver init order |
| Capture utility | 13 separate | Retain fake-port interruption/error/metadata tests; no real port opened |

The new headers contain argument/result recording only, not a duplicate MAC, driver or chip state machine. They are guarded native-only and excluded from firmware include paths. S3 compilation checks the actual pinned RadioLib constants and Arduino variant. Host tests cannot verify that hardware accepts a configuration, TCXO voltage rises, DC-DC operates, RF switch changes, or an interrupt arrives.

## Frequency, antenna and capacity

The user retained **915.0 MHz** for initial research. No frequency change or regional channel plan is implemented. Regulatory review was deferred by request; retaining the bench setting does not establish permission for radiated operation. Existing checks before deployment remain applicable. BW125, SF7 data, reserved SF9 control, CR4/5, preamble8, explicit header, PHY CRC and private sync0x12 are retained.

SX1262 supports SF5–SF12 and additional bandwidths, while the previous LLCC68 BW125 limit was SF5–SF9. The project's active research range remains SF7–SF9 at BW125; broader chip capability does not authorize automatic adaptation. Neither the manufacturer's sensitivity figure at a different SF nor the datasheet's approximate21±1 dBm maximum describes this firmware's 10 dBm link performance. Coding rate, preamble and header choices affect airtime even when the MCU is faster.

The 915 MHz Moxon is accepted as the design baseline for this target, as requested. Its fabrication, feed loss, impedance/SWR, polarization and actual gain remain unmeasured. That acceptance is not a claim of matching at 920–923 MHz or measured gain. Require an IPEX1.0-compatible 50-ohm pigtail and terminated RF path before any transmit.

EIRP = conducted power + antenna gain - feed loss. Current configuration supplies one conservative 10 dBm value to both roles; it cannot calculate an unknown antenna gain or enforce a jurisdiction-specific role limit. Future role-specific caps need an explicit board/application configuration decision and tests; do not silently change NVS schema or claim compliance from conducted power alone.

For illustration, standard LoRa airtime calculation with the unchanged SF7/BW125/CR4/5/preamble8/CRC/explicit profile gives about 66.816 ms for the 28-byte HELLO and82.176 ms for the 37-byte ACK, before switching/processing/guard time. Five exchanges consume at least 0.745 s of airtime; 70 consume 10.429 s. These are analytical examples, not simulation results or capacity measurements. Beacon/retry/control overhead and separate uplink/downlink duty budgets would add to them. Gateway downlink load can be the constraint even when each end node sends infrequently.

A five-client pilot needs scheduling/association tests and additional boards; the current eight-session dedup cache does not establish70-node replay retention. One SX1262 receives one PHY at a time. SF switching, beacon timing, BUSY turnaround, ACK timing, clock drift and possible later Wi-Fi coexistence require measured budgets before M3–M8. No scheduler or scalability demonstration is implemented here.

## Risk register and physical acceptance

| Risk | Disposition / next evidence |
|---|---|
| Wrong delivered board or LF RF variant | Seller confirmation plus front/back V3.2 HF photos and MCU/radio markings; target is already fixed, not a model-selection question |
| TCXO, DIO2 or DC-DC mismatch | Source-supported settings implemented; measure startup/BUSY/IRQ and verify RF path on the actual PCB |
| UART/USB routing and modem-line reset | CP2102 default documented; verify enumeration, upload,115200 transcript, capture-open resets and stable initial/final counters |
| Vext/battery revision mismatch | No peripheral code added; reserve36/37 and follow the V3.2 circuit for future features |
| ERR-021 BUSY hang | Remains open; safe controlled injection and bounded mitigation decision required for unattended recovery |
| NVS startup formatting / cross-chip restore | Actual-board private ROM backup, layout/security check and explicit provisioning; never restore a C3 full image to S3 |
| Supply dips, overheating or RF open circuit | Stable5 V supply with margin; measure TX rail/current and terminate IPEX path |
| Antenna/EIRP assumptions | Design baseline retained; gain/feed/SWR and authorization remain separate physical/operator checks |
| Memory, entropy and dual-core timing | Read ELF/budgets, then measure heap/stack/IRQ/reset/entropy behavior; no PSRAM assumption |
| False confidence from native tests | Separate compiled code, simulated stimuli and physical results in every record |

Procurement checklist: two intended V3.2 HF units,902–928 MHz order option within manufacturer863–928 MHz HF range, S3FN8, SX1262,8 MB flash, IPEX1.0, frequency-compatible included antenna, data-capable USB-C cable, regulated 5 V supply, matching pigtail,50-ohm RF load and VNA/equivalent. Receipt quantities and board-specific revisions are NOT AVAILABLE.

First inspect and privately back up each actual board in ROM. Confirm the S3 flash/security/partition layout before upload. With valid configuration the integrated radio may initialize and transmit, so a standalone-board procedure cannot assume an absent/disconnected radio. Keep RF terminated throughout. Provision Gateway1/1/1 and Client1/2/1 explicitly, verify role activation only after reboot and invalid config suppresses application TX. Then measure supply, reset, DIO1/BUSY and run the normal closed-cohort HELLO/ACK procedure with at least200 complete transmissions. Preserve raw captures, final drop counters, duplicate/negative-identity fixtures and timeout/recovery evidence. M0–M2 physical gates remain open until those observations exist.

The implementation sequence is now: review sources and scope; replace only the hardware composition/configuration; execute baseline/regression/new-adapter checks; review complete diff/history; publish through a normal-merge PR; then perform actual-board acceptance. Firmware migration at the source/build level does not complete physical M0–M2, resolve ERR-021 or begin M3.
