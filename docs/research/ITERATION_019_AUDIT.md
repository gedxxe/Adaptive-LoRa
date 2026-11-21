# Iteration 019 consistency audit

The starting tree was the published Iteration 018 commit
`0735bb8ef5cb7c1d677c98f2123c4a3f6cb7a1ea`. The review covered tracked
source, build inputs, test fixtures, the active English and Chinese docs,
reference assets, and prior result manifests. See [build memory](../en/BUILD_MEMORY.md)
for exact commands and the test-failure chronology.

| Area | Finding and disposition |
|---|---|
| PHY and hardware | The starting adapter requested 10 dBm. Iteration 019 requests 22 dBm. RadioLib 7.8.1 restores 60 mA OCP after PA selection, so the adapter now applies the SX1262 high-power 140 mA setting with error propagation before enabling IRQs. The native shim verifies the effective `begin()` arguments and protection call. V3.4 hardware continues to use the V3.2 electrical reference, existing GPIOs and one SX1262. Both profiles use SF7. The board SKU, antenna, actual conducted output and supply margin require inspection or measurement. |
| Provisioning and timing | The old default was 5,000 ms, bench instructions used 15,000 ms and the ACK window was 1 s. The user confirmed a 10,000 ms reporting default and 10,000 ms ACK timeout. Both bench roles explicitly set 10,000 ms. OLED freshness is two intervals plus one ACK window. A valid NVS blob retains its saved values and schema 1, including legacy 5,000 ms and bench 15,000 ms; the same old blob still decodes. M2 continues to send its first HELLO after the existing 1 s startup delay and measures later intervals from transaction completion. The v2 period is measured from beacon start to the next planned start. |
| v1 wire and state | The 28-byte header, 96-byte cap, little-endian serialization, explicit PHY header and CRC, message IDs, ACK echo and volatile HELLO deduplication remain compatible. The v1 codec, original golden frames, session tracker and M2 role source are unchanged. M2 opens a 10 s ACK window on serviced TX_DONE and does not retry that HELLO. v2 clears the ambiguity around reserved types by using a separate version and build profile. Each decoder rejects the other version. |
| Scheduled roles | There was no firmware beacon, DATA, or bitmap path. `FirmwareRuntime` now selects the v2 role logic in the research image using the same NVS and radio services. `ScheduledLink` validates peer, period, frame and session, derives a per-payload calendar, rejects off-slot traffic and performs one initial attempt plus up to three retries. Cumulative feedback preserves the accepted client identity and duplicate DATA is not accepted again. The v2 build excludes OLED work. |
| Driver and dependencies | `RadioDriver` ownership, IRQ latching, RX buffer checks, and driver TX deadline remain unchanged. The one-client guard is experimental. Polling deadlines cannot preempt a blocked RadioLib BUSY operation, and configuration save/SDK NVS initialization can delay service. ERR-021 and ERR-023 remain open. The core, Arduino, IDF, RadioLib and toolchain versions are in the build logs. |
| Test and analysis scope | Rebased timing fixtures on the restored settings while retaining the legacy NVS golden blob. The actual scheduled role implementation is exercised with a fake half-duplex transport. The 1/10/70 bitmap sizing checks are generic format checks. The Iteration 015 216-case model remains a multiclient analytical comparator with different inputs and assumptions. It provides no v2 capacity or field evidence. |
| Docs and history | Updated the active bilingual README, protocol, hardware, bench procedures and progress. The English architecture's current adapter table and Chinese M1 test plan still named LLCC68 and now name SX1262. Older audits retain their hardware, parameters and results. The v2 specification supersedes the earlier candidate layout. Captions, source hashes, existing figures and license records were retained. The four PDFs matched their registered SHA-256 values and remain local. |

The v2 example payload is a local timestamp. A general application delivery
API, authenticated or persistent replay protection, multi-client allocation,
PHY changes and ADR remain open. Bench work still needs a verified same-board
backup and authorized RF setup, followed by the M2 smoke test and separate
v2 timing and packet-loss measurements. No physical operation was performed.
