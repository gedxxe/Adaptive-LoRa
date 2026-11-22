# Build and protocol versions

| Use | PlatformIO environment | Wire version |
|---|---|---:|
| Adaptive walk test | `heltec_wifi_lora_32_V3_adaptive` | 4 |
| Fixed-SF, multi-Client comparison and project default | `heltec_wifi_lora_32_V3_multiclient` | 3 |
| Single-Client TDMA comparison | `heltec_wifi_lora_32_V3_research` | 2 |
| HELLO/ACK comparison | `heltec_wifi_lora_32_V3` | 1 |

Flash every board in a test with the same environment. These wire
versions are incompatible. `VERSION` records the firmware release label.
`PROTOCOL_VERSION` records the HELLO/ACK format. The scheduled builds
declare their versions in the respective codec headers.
[Packet formats](SCHEDULED_LINK.md)

Change the wire version when a layout, field encoding or message meaning
breaks compatibility. The adaptive build binds a DATA SF to the current
Gateway boot ID and frame. It does not save that SF to NVS.
Use the build ID or SHA-256 to identify the image used in a test.

<details>
<summary>Earlier release plan</summary>

Suggested milestone mapping:

| Version         | Milestone                                  |
|-----------------|--------------------------------------------|
| `0.1.0-alpha.1` | Radio bring-up                            |
| `0.2.0-alpha.1` | Fixed-PHY HELLO/ACK                        |
| `0.3.0-alpha.1` | Beacon and one-client TDMA                 |
| `0.4.0-beta.1`  | ACK bitmap and selective retry             |
| `0.5.0-beta.1`  | Versioned ADR and synchronization recovery |
| `0.9.0-rc.1`    | Pilot candidate                            |
| `1.0.0`         | Stable baseline after acceptance           |

This table is a proposed release sequence, not a statement that any
milestone is complete.

</details>
