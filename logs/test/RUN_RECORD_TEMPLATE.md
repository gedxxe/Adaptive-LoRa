# Single-board / two-device M0–M2 physical test record — NOT RUN

Copy into a new run directory as `RUN_RECORD.md`. Never overwrite an earlier run.

- Mode: MCU ONLY / TWO-DEVICE / CONTROLLED FIXTURE (select; NOT RUN)
- Local date/time (WIB), operator alias: TBD
- Firmware version / BUILD_ID / protocol: TBD
- Actual flashed Git branch / commit (distinct from capture script): TBD
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
- Capture start/end / actual monotonic duration / final status tail / port-open/close resets: TBD
- Backup/security/partition layout/NVS baseline SHA and restore verification (private references only): NOT RUN
- Intentional NVS erase/write scope and operator decision, or NOT AUTHORIZED: TBD
- MCU-only: no powered/connected radio throughout (physical inspection): NOT RUN
- ERR-021 approved bounded recovery disposition / current hold: OPEN, NOT VERIFIED
- Instruments/calibration/sample rate/current-limit/physical power-removal method: TBD

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
- Declared closed cohort/boundary exclusions/cumulative counter reconciliation: TBD
- A=all API attempts including failed starts; T=HELLO TX completions; R=unique Gateway receives; C=accepted ACKs: NOT MEASURED
- ACK transaction success=|C|/|A|; PDR=|R intersect T|/|T|; unpaired anomalies separately; zero denominator=N/A: NOT MEASURED
- RSSI/SNR distribution (per side) / transaction latency: NOT MEASURED
- MCU resets / driver/CRC/decode errors / log drop count: NOT MEASURED
- Raw files: `board.log` or `gateway.log`/`client.log`, per-stream `.chunks.jsonl`, `capture.json` state/hash/offset checks (TBD)
- Initial/final STATUS drop counts and boot/session continuity; partial-line/empty-stream/error review: TBD
- CSV method/version, HELLO identity associations, missing/ambiguous records, normal vs intervention cohort separation: TBD
- Negative ACK identity and duplicate RF injection fixtures/logs: NOT RUN (native results are separate)
- Result, issue IDs, unresolved assumptions, next action: TBD
