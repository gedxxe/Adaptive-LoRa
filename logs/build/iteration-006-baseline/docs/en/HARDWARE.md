# Hardware and RF Baseline

## 1. Current inventory

| Item               | Specification                                 |         Quantity | Status     |
|--------------------|-----------------------------------------------|-----------------:|------------|
| Gateway MCU        | ESP32-C3 Super Mini                           |                1 | Available  |
| Client MCU         | ESP32-C3 Super Mini                           |                1 | Available  |
| Gateway radio      | Ai-Thinker Ra-01SCH, LLCC68                   |                1 | Available  |
| Client radio       | Ai-Thinker Ra-01SCH, LLCC68                   |                1 | Available  |
| Antenna            | 915 MHz Moxon                                 | Confirm quantity | Under test |
| RF cable/connector | Match the actual module and antenna connector |              TBD | Verify     |

Both devices use the same hardware. Device role is selected through NVS
configuration.

## 2. Module identification

The module is Ai-Thinker Ra-01SCH based on LLCC68.

Use:

- RadioLib `LLCC68`.
- SPI communication.
- DIO1 for radio event notification.
- BUSY for command readiness.
- RESET for module reset.

Do not use RadioLib `SX1262` for this module. Do not confuse it with a
Heltec board containing an SX1262.

Recorded module constraints:

- Supply: 2.7–3.6 V.
- Frequency range: 803–930 MHz.
- BW125: SF5–SF9.
- BW250: SF5–SF10.
- BW500: SF5–SF11.
- SF12 unsupported.
- Packet engine: up to 256 bytes.

Verify the exact module marking and revision before relying on this
record.

## 3. Proposed pin map

This is a proposed mapping for the ESP32-C3 Super Mini and Ra-01SCH. It
is not a substitute for checking the actual board schematic and pin
labels.

|       ESP32-C3 GPIO | Ra-01SCH pin | Signal   |
|--------------------:|-------------:|----------|
|               GPIO4 |           12 | SCK      |
|               GPIO5 |           13 | MISO     |
|               GPIO6 |           14 | MOSI     |
|               GPIO7 |           15 | NSS / CS |
|               GPIO1 |            4 | RESET    |
|               GPIO0 |            6 | DIO1     |
|              GPIO10 |           10 | BUSY     |
|      External 3.3 V |            3 | VCC      |
|                 GND |     2, 9, 16 | Ground   |
| 50-ohm antenna feed |            1 | ANT      |

Before wiring:

- Photograph both board silkscreens.
- Confirm the module pin numbering orientation.
- Confirm continuity from MCU pads to the intended GPIOs.
- Check that no pin is already used by onboard LEDs, USB, or boot
  circuitry.
- Record the actual mapping in the build iteration record.

Ra-01SCH pins 5 (TXEN) and 11 (RXEN) are RF switch controls handled
internally by the standard module. Leave them unconnected unless the
exact breakout documentation says otherwise.

ESP32-C3 cautions:

- GPIO2, GPIO8, GPIO9 are strapping pins.
- GPIO18/19 are associated with USB Serial/JTAG.
- GPIO20/21 are UART0.
- GPIO4–GPIO7 overlap external JTAG functionality.

## 4. Power design

The radio requires a stable 3.3 V supply. Do not assume the MCU board’s
onboard 3V3 regulator can handle radio TX transients.

Initial recommendation:

- Dedicated regulated 3.3 V rail for the radio.
- Regulator rating at least 300 mA, with margin after verifying actual
  current demand.
- Common ground between MCU and radio.
- Local ceramic decoupling close to the radio supply pins.
- Short supply and ground paths.
- Measure voltage at the module during transmit.

Never:

- Apply 5 V to the radio VCC.
- Exceed 3.6 V.
- Transmit without an antenna or suitable 50-ohm load.
- Increase TX power to compensate for an unverified antenna or poor
  supply.

Start at 10 dBm for bench validation.

## 5. Initial RF profile

| Parameter   |                Value | Note                                        |
|-------------|---------------------:|---------------------------------------------|
| Frequency   |            915.0 MHz | Provisional test setting; check local rules |
| Bandwidth   |              125 kHz | Fixed for M0–M5                             |
| Control SF  |                  SF9 | Provisional common control PHY              |
| Data SF     |                  SF7 | Initial data PHY                            |
| ADR range   |              SF7–SF9 | Future milestone                            |
| Coding rate |                  4/5 | Initial                                     |
| Preamble    |                    8 | Initial                                     |
| Header      |             Explicit | Initial                                     |
| CRC         |              Enabled | Required                                    |
| Sync word   |                 0x12 | RadioLib one-byte private LoRa sync word    |
| TX power    |               10 dBm | Initial bench setting                       |
| SPI clock   |                1 MHz | Initial conservative setting                |

At BW125, LLCC68 does not support SF10–SF12. Do not put unsupported
combinations in configuration tables or test loops.

Candidate module settings are XTAL (`tcxoVoltage=0`) and LDO regulator mode. RadioLib default DIO2 RF-switch control is retained for the module's internal path; MCU TXEN/RXEN are not connected. Verify these choices on the exact module/breakout. [Pinned LLCC68 API](https://github.com/jgromes/RadioLib/blob/7.8.1/src/modules/LLCC68/LLCC68.h) defines the initialization arguments. The API does not expose its matched identification string; serial status reports driver selection and initialization result, not independent silicon detection.

The candidate enables native USB CDC. Exact USB routing, flash size and board revision remain provisional. On 2026-11-12 only six Bluetooth ports were enumerated (`logs/test/iteration-004-serial-ports.log`); no ESP32 USB/USB-UART port, photos, schematic or continuity evidence was available. No pins or supply readings are confirmed by the successful compile.

## 6. Frequency and antenna compliance

The 915 MHz Moxon is within the module’s hardware frequency range. This
does not establish that 915.0 MHz, the selected channel width, TX power,
or antenna gain is permitted for the intended operation in Indonesia.

Before any operational deployment, verify:

- permitted frequency range and channel plan;
- maximum conducted power and EIRP;
- antenna gain and feed-line loss;
- duty-cycle, channel-access, or other applicable access requirements;
- equipment certification and site requirements.

Keep 915.0 MHz as a provisional bench value until that review is
complete.

## 7. Antenna test controls

For a meaningful comparison:

- Use the same radio pair and PHY settings.
- Keep TX power fixed.
- Keep device locations and heights fixed.
- Record antenna orientation and polarization.
- Record LOS/NLOS and machinery state.
- Do not compare measurements taken at different distances as if they
  were a controlled antenna comparison.
- Record packet count and test duration.
- Capture RSSI, SNR, PDR, ACK success, retries, and latency.
- Use a suitable reference antenna if one is available; do not invent a
  baseline.

A Moxon design or 3D-printed jig is not evidence of antenna gain or
impedance match. Measure or validate RF performance before making gain
claims.
