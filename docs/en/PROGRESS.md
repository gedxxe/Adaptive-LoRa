# Results and research

[Home](../../README.md) · [Field test](BENCH_RUNBOOK.md) · [Protocol](SCHEDULED_LINK.md) · [简体中文](../zh-CN/PROGRESS.md)

<!-- current-state:start -->
| Check | Result |
|---|---|
| Firmware / protocol | `0.2.0-alpha.1` / `4` |
| Board | Heltec WiFi LoRa 32 V3.4, ESP32-S3/SX1262 |
| Adaptive image | `heltec_wifi_lora_32_V3_adaptive`, ESP32 build passed, 8/8 native cases |
| Fixed-SF image | `heltec_wifi_lora_32_V3_multiclient`, ESP32 build passed, 15/15 runtime cases |
| Shared tests | 356/356 native cases for Iteration 022 |
| Field test | Walk test and radio timing measurements pending |
<!-- current-state:end -->

Host cases cover mixed-SF operation, a roaming Client with stationary
peers, the eight-Client reservation, ACK receipt and radio recovery.
The host results do not establish range or timing on the boards.
[Build and test records](BUILD_MEMORY.md)

The airtime calculation for eight 64-byte limits and the other Clients
at SF7 gives **14.156640 s** with two SF9 reservations and **15.634272 s**
with three. Compare these model results against the configured period,
then measure turnaround and drift on the boards.
[Model and settings](SCHEDULED_LINK.md#airtime-and-slot-budget)

## Papers and comparison runs

[Research-to-code map](../research/ALGORITHM_TRACEABILITY.md) ·
[Paper references and figure sources](../references/README.md)

| Use | PlatformIO environment | Wire version |
|---|---|---:|
| Adaptive field test | `heltec_wifi_lora_32_V3_adaptive` | 4 |
| Fixed-SF comparison and PlatformIO default | `heltec_wifi_lora_32_V3_multiclient` | 3 |
| Single-Client TDMA comparison | `heltec_wifi_lora_32_V3_research` | 2 |
| HELLO/ACK comparison | `heltec_wifi_lora_32_V3` | 1 |

Use the same wire version on every board in a run. To isolate DATA SF's
effect, match the control PHY, superframe, retry policy, antenna placement
and load. The shipped comparison images differ in some of those settings.

## Past work

[Earlier manuals, hardware checks and test plans](https://github.com/gedxxe/Adaptive-LoRa/tree/4ab3655dc8ca3b7f9ddc382d8ebeeeadceb6d2c7/docs).
This pinned Git snapshot keeps the documents from before consolidation.
The project also keeps its [build log](BUILD_MEMORY.md),
[decisions](DECISIONS.md) and [error log](ERROR_LOG.md).
