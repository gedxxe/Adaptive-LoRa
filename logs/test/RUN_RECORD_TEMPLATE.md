# Two-device M0–M2 physical test record — NOT RUN

Copy into a new run directory as `RUN_RECORD.md`. Never overwrite an earlier run.

- Local date/time (WIB), operator: TBD
- Firmware version / BUILD_ID / protocol: TBD
- Git branch / commit: NOT AVAILABLE unless initialized later
- Firmware binary SHA-256: TBD
- Gateway role / network / node / gateway IDs: TBD
- Client role / network / node / gateway IDs: TBD
- Board revisions / flash / USB paths (both units): TBD
- Module markings / pin orientation / continuity evidence: TBD
- Radio idle and TX rail voltage / regulator / decoupling: NOT MEASURED
- Antenna(s), connector, feed line, matching evidence: TBD
- Frequency/channel/power/site authorization review: TBD
- PHY / sync word / TX power / interval: TBD
- Distance / LOS-NLOS / heights / polarization / orientation / machinery: TBD
- Capture start/end / duration / port-open resets: TBD

| Check | Gateway | Client | Evidence |
|---|---|---|---|
| Missing role: provisioning and no application TX | NOT RUN | NOT RUN | TBD |
| Invalid complete config refused | NOT RUN | NOT RUN | TBD |
| Save/readback/power-cycle persistence | NOT RUN | NOT RUN | TBD |
| Role edits deferred until reboot | NOT RUN | NOT RUN | TBD |
| Corrupt/unsupported NVS fixture: no TX | NOT RUN | NOT RUN | TBD |
| LLCC68 init/status, BUSY, DIO1, reset | NOT RUN | NOT RUN | TBD |
| Stable serial/no dropped logs | NOT RUN | NOT RUN | TBD |

- HELLO attempts / gateway valid unique / duplicate / stale: NOT MEASURED
- Gateway ACK TX starts / TX completions: NOT MEASURED
- Client matching ACKs / ACK timeouts / TX failures: NOT MEASURED
- ACK success = accepted / attempted; PDR = unique gateway HELLO / completed client HELLO TX (state denominator): NOT MEASURED
- RSSI/SNR distribution (per side) / transaction latency: NOT MEASURED
- MCU resets / driver/CRC/decode errors / log drop count: NOT MEASURED
- Raw files: `gateway.log`, `client.log`, `capture.json` (TBD)
- Negative ACK identity and duplicate RF injection fixtures/logs: NOT RUN (native results are separate)
- Result, issue IDs, unresolved assumptions, next action: TBD
