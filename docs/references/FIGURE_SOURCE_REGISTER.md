# Figure source register

The [architecture](../research/figures/adaptive-architecture.svg) and
[frame sequence](../research/figures/adaptive-frame.svg) are original
project drawings based on the adaptive implementation. The example SFs
and packet positions illustrate how the code works. They are not
measurements. The antenna symbol is also original.

The [group rotation](../research/figures/rotation.svg) and [profile badges](../research/figures/project-profile.svg)
are original project SVGs. The rotation figure explains the registry cursor and
the frame phases. The badges name the hardware and tools in plain text.

The listings for [Algorithm 1 (Gateway scheduling)](../../README.md#algorithm-1-gateway-scheduling)
and [Algorithm 2 (Client delivery)](../../README.md#algorithm-2-client-delivery)
describe the [adaptive runtime](../../src/app/multiclient_link.cpp)
and its [wire-v7 authentication checks](../../src/app/secure_link.cpp). Both come from
one [LaTeX source](../research/figures/adaptive-algorithm.tex). To rebuild
them from the repository root, use pdfLaTeX with `standalone` and AMS packages,
and Poppler. This assumes `.pio` exists.

```sh
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=.pio docs/research/figures/adaptive-algorithm.tex
pdftoppm -r 288 -png -gray -f 1 -l 1 -singlefile .pio/adaptive-algorithm.pdf docs/research/figures/adaptive-algorithm-gateway
pdftoppm -r 288 -png -gray -f 2 -l 2 -singlefile .pio/adaptive-algorithm.pdf docs/research/figures/adaptive-algorithm-client
```

The [v3 implementation diagram](../research/figures/v3-superframe.svg) was
drawn for Iteration 020 from the production calendar and bitmap layout. The
paper figures and the earlier explanatory drawings below remain available
with their original attribution and adaptations.

[Reference entry point and full citations](README.md). PDF index is zero-based
in the supplied local file. Printed page is the visible article page, which
is independent of the PDF index. Captions transcribe the local PDFs with line
breaks joined and displayed subscripts rendered as Unicode where possible.
The source files are retained locally and are outside the publication tree.

| Ref. | Figure | Original caption | PDF index | Printed page | Section | License and use | Asset | Adaptation | Firmware or host mapping |
|---|---:|---|---:|---|---|---|---|---|---|
| R1 | 1 | A collection of superframes spanning different SFs and payload ranges (PL1–k). | 2 | 148 | §§3.1–3.2 | CC BY-NC-ND 4.0 (paper). Original SVG only. | [asset](figures/tsvp/tsvp-superframe-redrawn.svg) | Modified. Single-radio serial plan | tools/tdma_feasibility.py::schedule_budget (host budget) |
| R1 | 2 | Structure of a superframe for payload range 𝑖 in the TS-VP-LoRa scheme. | 3 | 149 | §3.2 | CC BY-NC-ND 4.0 (paper). Original SVG only. | [asset](figures/tsvp/tsvp-superframe-redrawn.svg) | Modified. Measured guard inputs | tools/tdma_feasibility.py::schedule_budget (guard input only) |
| R1 | 3 | Structure of beacons in the TS-VP-LoRa scheme. | 4 | 150 | §3.2 | CC BY-NC-ND 4.0 (paper). Original SVG only. | [asset](figures/tsvp/tsvp-beacon-redrawn.svg) | Modified. Versioned schedule and per-frame grant | [scheduled_link.cpp](../../src/app/scheduled_link.cpp) sends and validates the v2 beacon. |
| R2 | 3 | The system model of the considered LoRa network. | 7 | 8 | §3.1 | © 2023 Elsevier B.V., all rights reserved. Original SVG only. | [asset](figures/blind-adr/blind-adr-frame-redrawn.svg) | Comparator. Mobile concentrator network | [sx1262_hardware.cpp](../../src/common/radio/sx1262_hardware.cpp) `Sx1262Hardware::begin selects one PHY` |
| R2 | 4 | Time-slotted frame structure of proposed scheme. | 8 | 9 | §3.2 | © 2023 Elsevier B.V., all rights reserved. Original SVG only. | [asset](figures/blind-adr/blind-adr-frame-redrawn.svg) | Modified. Gateway slot assignment, no random slots | [scheduled_link.cpp](../../src/app/scheduled_link.cpp) grants a fixed slot; ADR is planned. |
| R2 | 5 | The variation of RSSI with respect to the distance from GW. | 10 | 11 | §4 | © 2023 Elsevier B.V., all rights reserved. Original SVG only. | [asset](figures/blind-adr/blind-adr-sf-selection-redrawn.svg) | Modified. Measured link evidence, no distance model | [radio_driver.cpp](../../src/common/radio/radio_driver.cpp) `RadioDriver::poll collects RX metrics` |
| R2 | 6 | The distribution of SFs with regarding to distance from GW. | 10 | 11 | §4 | © 2023 Elsevier B.V., all rights reserved. Original SVG only. | [asset](figures/blind-adr/blind-adr-sf-selection-redrawn.svg) | Comparator. No published SF thresholds | [radio_config.h](../../include/radio_config.h) `phy::dataSf` (fixed M2 value) |
| R3 | 1 | Architecture and Concurrency Limits of a COTS LoRa Gateway. | 5 | 10:6 | §3 | CC BY 4.0. Attributed original crop. | [asset](figures/lorahart/lorahart-fig-01-gateway.png) | Adopted principle. Half-duplex sequencing | [radio_driver.cpp](../../src/common/radio/radio_driver.cpp) `RadioDriver::receive/transmit/poll` |
| R3 | 2 | LoRaHART Super-frame Structure. | 8 | 10:9 | §5 | CC BY 4.0. Attributed original crop. | [asset](figures/lorahart/lorahart-fig-02-superframe.png) | Modified. Serialized uplinks and deterministic retry slots | [scheduled_protocol.h](../../src/common/protocol/scheduled_protocol.h) defines the one-client calendar. |
| R3 | 3 | Multicast Acknowledgment. | 9 | 10:10 | §5 | CC BY 4.0. Attributed original crop. | [asset](figures/lorahart/lorahart-fig-03-ack-bitmap.png) | Modified. Schedule-order bitmap and version binding | [scheduled_protocol.cpp](../../src/common/protocol/scheduled_protocol.cpp) validates the v2 bitmap and identity fields. |
| R4 | 1 | Dividing the field into overlapping coronas with variable SFs. | 2 | 12286 | §III-A (introduced in §III) | © 2024 IEEE, personal use. Republication/redistribution requires permission. Original SVG only. | [asset](figures/ats-lora/ats-corona-redrawn.svg) | Comparator. Geographic SF zones not adopted | [radio_config.h](../../include/radio_config.h) `phy::dataSf remains fixed` |
| R4 | 2 | Equal radius-based corona (ERBC) mode example of Corona Cᵢ with radius Rᵢ. | 3 | 12287 | §III-A.1 | © 2024 IEEE, personal use. Republication/redistribution requires permission. Original SVG only. | [asset](figures/ats-lora/ats-corona-redrawn.svg) | Comparator. Equal radial increments | No coordinate-based allocator |
| R4 | 3 | Equal area-based corona (EABC) mode example of Corona Cᵢ with radius Rᵢ. | 4 | 12288 | §III-A.2 | © 2024 IEEE, personal use. Republication/redistribution requires permission. Original SVG only. | [asset](figures/ats-lora/ats-corona-redrawn.svg) | Comparator. Equal-area rings | No coordinate-based allocator |
| R4 | 4 | Equal area-based sector (EABS) mode example of Corona Cᵢ with radius Rᵢ. | 4 | 12288 | §III-A.3 | © 2024 IEEE, personal use. Republication/redistribution requires permission. Original SVG only. | [asset](figures/ats-lora/ats-corona-redrawn.svg) | Comparator. Equal-area sectors | No coordinate-based allocator |
| R4 | 6 | Comparison between the frame sizes of ATS-LoRa protocol in ERBC, EABC, and EABS modes and SBTS-LoRa protocol [13]. (a) ATS-LoRa Protocol. (b) SBTS-LoRa Protocol. | 6 | 12290 | §III-B; presented on §IV opening page | © 2024 IEEE, personal use. Republication/redistribution requires permission. Original SVG only. | [asset](figures/ats-lora/ats-frame-allocation-redrawn.svg) | Comparator. Frame budgeting without geographic allocation | tools/tdma_feasibility.py::schedule_budget (host budget only) |

## Source identity and reproduction

| Ref. | Local filename | SHA-256 |
|---|---|---|
| R1 | `Time-slotted LoRa MAC with variable payload support.pdf` | `f7cd4971408926ad0a8b64cfa67e8c71c0335a55b760490fe359656c4dcc2236` |
| R2 | `An adaptive spreading factor allocation scheme for mobile LoRa networks.pdf` | `557cb8044c6e67b7f595e10ed14ad576c1fff1b787a4582909dd9d7ab7453147` |
| R3 | `LoraHART Paper reference.pdf` | `5741fcf90705e366ac923745b3420e8ffa0044174a58c091e491c4090c17b6d4` |
| R4 | `A_Novel_Autonomous_Adaptive_Frame_Size_for_Time-Slotted_LoRa_MAC_Protocol.pdf` | `9085bf42097f2ad21afd4ddf681eeadc610d31eed8a7e5292188e525e342b021` |

R1’s first page declares [CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/).
R2’s first page reserves Elsevier’s rights. R4’s first page permits personal
use and requires permission for republication or redistribution. The six SVGs
explain the underlying methods with original composition. They contain no
source image or plotted paper data.

R3’s first page and the [publisher record](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ECRTS.2025.10)
identify the authors and [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
The three PNGs are unscaled crops of the 600 DPI Poppler render. Figure 1
includes both panels and both subplots. Figure 2 includes all phases, labels
and the total-frame arrow. Figure 3 includes the header, payload and node-ID
labels. Only surrounding page material and the captions were excluded.

| R3 figure | Render with `pdftoppm -r 600 -png -singlefile` | Crop in PDF points (left, top, right, bottom) | PNG pixels |
|---|---|---|---|
| 1 | `-f 6 -l 6` | 113, 68, 494, 190 | 3175 × 1016 |
| 2 | `-f 9 -l 9` | 181, 70, 413, 167 | 1934 × 809 |
| 3 | `-f 10 -l 10` | 207, 70, 426, 110 | 1825 × 334 |

Crop coordinates use a top-left origin. To repeat, multiply each coordinate
by 600/72 and round to the nearest pixel. Save the cropped PNG with 600 DPI
metadata. Attribution and a description of the cropping are supplied beside
the images in the [reference entry point](README.md).
