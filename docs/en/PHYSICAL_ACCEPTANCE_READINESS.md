# M0–M2 physical acceptance readiness

The owner confirms two Heltec WiFi LoRa 32 V3.4 boards with ESP32-S3, SX1262,
stock monopoles, the 902–928 MHz procurement variant and V3.2 wiring. The
V3.2 schematic stays the electrical reference. Physical SKU labels, antenna
markings and characteristics, USB, flash, supply, OLED and RF timing await
bench checks. Software build and native evidence are recorded separately.

Start with one board at a time. Inspect each RF connector, attach a suitable
load before first power and identify the CP2102 COM port with a direct data
cable. Preserve a private same-board backup from ROM and review the flash
layout/security before upload. Follow the [bench runbook](BENCH_RUNBOOK.md)
for the opt-in OLED image, explicit 15,000 ms provisioning on each role and
an authorized two-board smoke capture before any range survey.

ERR-021 BUSY recovery and ERR-023 SDK startup formatting remain open.
M0–M2 physical acceptance is in progress. M3 depends on M2 evidence and
measured timing.
