# Hardware and RF baseline

The active target is **Heltec WiFi LoRa 32 V3.2 HF**, with ESP32-S3FN8,
SX1262,8 MB SiP flash,512 KB SRAM and CP2102. No PSRAM is assumed.
The procurement target is confirmed, but delivery, individual PCB markings
and silicon stepping have not been inspected. The first acceptance setup
needs two identical boards, one configured Gateway and one Client.

Use the HF version, not the470-510 MHz LF version. Manufacturer HF coverage
is863-928 MHz; the ordered902-928 MHz option is within that range. V4,
Wireless Stick Lite and Tracker are different products. Require seller
confirmation and clear front/back V3.2 HF photographs. Source URLs,
fingerprints, revision comparison and the complete GPIO matrix are in
[HARDWARE_MIGRATION_AUDIT.md](HARDWARE_MIGRATION_AUDIT.md).

## Radio and reserved resources

| Signal | ESP32-S3 GPIO |
|---|---:|
| NSS | 8 |
| SCK | 9 |
| MOSI | 10 |
| MISO | 11 |
| RESET | 12 |
| BUSY | 13 |
| DIO1 | 14 |

This mapping is checked against the V3.2 schematic and Arduino variant;
confirm that the delivered board matches before use. DIO2 drives the onboard
RF switch and DIO3 supplies the32 MHz TCXO. RadioLib 7.8.1 uses explicit
TCXO1.8 V and DC-DC selection, matching the vendor code and populated
L8/C54 network. Do not assign additional MCU TXEN/RXEN or TCXO-control pins.
Only Sx1262Hardware owns the radio/SPI/GPIO/IRQ adapter.

Reserve OLED17/18/21, LED35, Vext36, ADC_Ctrl37, CP2102 UART43/44 and PRG0.
GPIO19/20 have optional native-USB routing; R3/R29 are unpopulated in the
reference schematic. The normal USB-C connection uses CP2102. Do not join
two USB transceivers on the same data pair. Respect straps0/3/45/46,
flash/internal resources, crystal15/16 and external-JTAG choices.

V3.2 Vext is a switched LDO controlled active-low by GPIO36. The radio
and RF-switch bias use VDD_3V3; OLED uses Vext. This firmware leaves OLED,
Vext control and battery sensing unused. Later display startup must set a
safe output latch, enable the rail, allow proper settling, then release
OLED reset. GPIO37 high/pull-up enables the revised battery-divider path;
earlier revision logic must not be copied. The charge IC is LGS4056HDA.

## Power, RF feed and handling

Use a stable regulated 5 V supply with margin. The manufacturer specifies
at least 500 mA input capability for USB/5 V and 150 mA for direct3.3 V.
These are input specifications, not measured project current or spare
external-load capacity. Measure voltage/current during TX; check for resets,
connector polarity, heating and supply transients. Do not parallel arbitrary
supply outputs or back-power an unpowered board through GPIO.

The LoRa connector is IPEX1.0. Fit a compatible 50-ohm pigtail and appropriate
antenna or RF load **before power**, including single-board and first-boot
work. The integrated radio cannot be unplugged as a separate module, and
an unknown factory image or valid saved role may transmit. Never transmit
with an open RF connector. Start at 10 dBm; inspect the power/feed/placement
before changing power to compensate for a link problem.

## Fixed research PHY

| Parameter | Setting |
|---|---|
| Frequency | 915.0 MHz, provisional research setting |
| Bandwidth | 125 kHz |
| Data SF | SF7 |
| Reserved control SF | SF9; unused in M2 |
| Coding rate | 4/5 |
| Preamble | 8 symbols |
| Header / PHY CRC | Explicit / enabled |
| Private sync | RadioLib one-byte0x12 |
| TX power | 10 dBm |
| SPI | 1 MHz, mode0, MSB first |
| TCXO / radio regulator | DIO3 1.8 V / DC-DC |

SX1262 supports SF5-SF12, but this research baseline remains SF7-SF9 at
BW125. No dynamic PHY, TX-power adaptation or scheduler is added. Retaining
915 MHz at the user's request does not establish permission for radiated
operation; applicable channel/EIRP/access/equipment checks remain necessary
before deployment. EIRP equals conducted power plus antenna gain minus feed
loss, which cannot be calculated honestly until the antenna/feed are known.

The 915 MHz Moxon is accepted as the design baseline. Fabrication, impedance,
SWR, gain and actual radiation pattern remain unmeasured; no matching claim
for920-923 MHz follows. Use identical radios, PHY, power, distance, placement,
polarization, feed and run duration for a later reference-versus-Moxon
comparison. Preserve packet/ACK counts and RSSI/SNR distributions; changing
location or distance invalidates a claimed controlled comparison.

## Bring-up evidence still required

Follow [BENCH_RUNBOOK.md](BENCH_RUNBOOK.md). Confirm received PCB/RF variant,
flash/security/USB routing, supply, RF termination, TCXO/BUSY/DIO1/reset,
serial stability and the actual HELLO/ACK link. Host tests and successful
S3 compilation have not established those physical properties. ERR-021
still blocks unattended stuck-BUSY recovery acceptance; no library patch
or watchdog change is included. SDK startup can format NVS before the
runtime (ERR-023), so same-board private backup/recovery comes before
application boot or intentional configuration changes.
