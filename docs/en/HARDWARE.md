# Hardware and radio settings

The bench pair is two **Heltec WiFi LoRa 32 V3.4** boards with ESP32-S3,
SX1262 and stock monopole antennas. The owner confirms the 902–928 MHz
procurement variant, the V3.2 electrical wiring and pin mapping, and V3.4
improvements to SMT implementation and the crystal oscillator. The V3.2
schematic remains the electrical reference for these boards.

The manufacturer V3 family lists ESP32-S3FN8, SX1262, 8 MB flash, CP2102 and
a 128 × 64 display. The published revision diagrams provide the V3.2 reference
and the installed Arduino V3 variant supplies the build pin aliases. The
owner confirmation identifies the research hardware. Physical SKU labels,
antenna markings/band/matching, board continuity, oscillator behavior,
USB/flash, supply and display behavior remain to be checked directly. No physical
measurement or board capture is recorded.

Sources: [Heltec V3.2 schematic](https://s.heltec.cn/download/WiFi_LoRa_32_V3/WiFi_LoRa_32_V3.2_Schematic_Diagram.pdf),
[Heltec product specifications and schematics](https://heltec.org/project/wifi-lora-32-v3/),
[Heltec hardware revision log](https://docs.heltec.org/en/node/esp32/wifi_lora_32/hardware_update_log.html)
and [Heltec V3 reference driver](https://github.com/HelTecAutomation/Heltec_ESP32/blob/master/src/heltec.cpp).
The [Iteration 014 migration audit](HARDWARE_MIGRATION_AUDIT.md) records the
V3.2 schematic and source details in their original context.

| Item | Reference evidence | Delivered V3.4 confirmation needed |
|---|---|---|
| Identity and PCB | Heltec V3 family | Front/back markings, layout, MCU and radio package markings, SKU and matching network |
| Radio SPI and interrupt | V3.2 schematic and installed variant. NSS 8, SCK 9, MOSI 10, MISO 11, RESET 12, BUSY 13, DIO1 14 (variant alias DIO0) | Compare nets and observe reset, BUSY and DIO1 with high-impedance probes |
| Oscillator and PA path | V3.2 reference has DIO3 TCXO at 1.8 V, DC-DC regulator and DIO2 RF switch | Verify oscillator marking/voltage, populated regulator and switch path |
| Display | V3 source selects SSD1306 128 × 64 at 7-bit 0x3C. Variant aliases are SDA 17, SCL 18, RESET 21 | Check reset, bus routing, address ACK and actual panel response |
| Vext | V3.2 switched LDO, GPIO36 active low. Radio uses VDD_3V3 and display uses Vext | Confirm polarity and rail settling before the first upload |
| USB and flash | V3 reference CP2102/UART0 TX43/RX44, 8 MB flash | Enumerate one board at a time, verify ROM identity/flash/security/layout |
| RF connector and antennas | V3 reference IPEX 1.0. Heltec offers several frequency variants | Verify each connector, RF SKU, monopole markings/band and matching |

The installed Arduino-ESP32 3.3.11 `heltec_wifi_lora_32_V3` variant defines
display aliases separately from default `Wire` pins 41/42. The firmware
uses the board aliases and asserts their values at compile time.
No existing radio pin constants were edited. Keep board/peripheral pin and
power assignments under their current owners. Reserve LED35, battery
control37, UART43/44, USB19/20, boot straps, crystal and flash/JTAG resources
as documented in the migration audit.

## Power, RF feed and handling

Use a stable regulated 5 V supply with margin. The manufacturer specifies
at least 500 mA input capability for USB/5 V and 150 mA for direct3.3 V.
These are input specifications, not measured project current or spare
external-load capacity. Measure voltage/current during TX; check for resets,
connector polarity, heating and supply transients. Do not parallel arbitrary
supply outputs or back-power an unpowered board through GPIO.

The reference LoRa connector is IPEX 1.0. Verify the delivered connector before mating it. Fit a compatible 50-ohm pigtail and appropriate
antenna or RF load **before power**, including single-board and first-boot
work. The integrated radio cannot be unplugged as a separate module, and
an unknown factory image or valid saved role may transmit. Never transmit
with an open RF connector. Measure supply voltage during TX with a suitable
50-ohm load and check feed and antenna placement before a range test.

## Radio settings

| Parameter | Setting |
|---|---|
| Frequency | 915.0 MHz, provisional research setting |
| Bandwidth | 125 kHz |
| DATA SF in the adaptive image | SF7–SF9, assigned per Client |
| Control SF in the adaptive image | SF9 |
| Coding rate | 4/5 |
| Preamble | 8 symbols |
| Header / PHY CRC | Explicit / enabled |
| Private sync | RadioLib one-byte0x12 |
| TX power / PA overcurrent protection | 22 dBm requested / 140 mA configured |
| SPI | 1 MHz, mode0, MSB first |
| TCXO / radio regulator | DIO3 1.8 V / DC-DC |

RadioLib 7.8.1 restores its default 60 mA
OCP after setting PA power. The adapter then applies the SX1262 high-power
140 mA OCP setting (Semtech datasheet, table 5-2) and checks the return code.
Neither value is a board measurement. Heltec specifies 21 ± 1 dBm for the V3
family. Verify actual conducted output, current, supply droop and heating
under the delivered hardware and load. Radiated tests require authorization
for the frequency, EIRP (conducted power plus antenna gain minus feed loss),
airtime and operating site. The planned Moxon is outside this bench iteration. Use the two stock
monopoles for the first link after confirming their markings and suitability.
See the [Semtech SX1262 documentation](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)
and [Heltec V3 specifications](https://heltec.org/project/wifi-lora-32-v3/).

## Checks on the delivered boards

Follow [BENCH_RUNBOOK.md](BENCH_RUNBOOK.md). Confirm received PCB/RF variant,
flash/security/USB routing, supply, RF termination, TCXO/BUSY/DIO1/reset,
serial stability and the actual HELLO/ACK link. Host tests and successful
S3 compilation have not established those physical properties. ERR-021
still blocks unattended stuck-BUSY recovery acceptance; no library patch
or watchdog change is included. SDK startup can format NVS before the
runtime (ERR-023), so same-board private backup/recovery comes before
application boot or intentional configuration changes.
