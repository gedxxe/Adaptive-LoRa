# Test Plan

## 1. Test levels

Keep evidence separate across:

1.  Static review.
2.  Native host unit tests.
3.  ESP32 compile.
4.  Single-device radio bring-up.
5.  Two-device RF test.
6.  Multi-node simulation.
7.  Multi-node physical test.

A result at one level does not prove the next level.

## 2. M0 — hardware and toolchain baseline

Record:

- exact ESP32-C3 Super Mini board revision and visible markings;
- exact Ra-01SCH module marking and pin orientation;
- GPIO-to-module continuity;
- radio supply voltage at idle and during TX;
- antenna connector and 50-ohm connection;
- antenna type, orientation, and physical placement;
- PlatformIO Core, espressif32 platform, Arduino-ESP32, compiler, and
  RadioLib versions;
- local frequency/power compliance review status.

Acceptance:

- no unverified pin is silently treated as confirmed;
- radio VCC is within 2.7–3.6 V;
- antenna is connected before any transmission;
- all toolchain versions are recorded.

## 3. M1 — radio bring-up

Run on each physical unit independently.

Verify:

- correct LLCC68 initialization;
- expected RadioLib status code;
- BUSY behavior;
- DIO1 event behavior;
- reset behavior;
- no repeated brownout or watchdog reset;
- stable serial output.

Acceptance:

- both devices initialize successfully with the same firmware image;
- each device reports the role loaded from NVS;
- no application TX occurs when role/config is invalid.

## 4. M2 — fixed-PHY HELLO / ACK

### Bench setup

- Start at a short, clear line-of-sight distance.
- Use the same fixed PHY on both ends.
- Initial TX power: 10 dBm.
- Use the 915 MHz Moxon only after confirming its connector and matching
  path.
- Record antenna orientation and supply voltage.
- Use serial logs from both devices with synchronized test start notes.

### Test cases

| ID     | Test                      | Expected result                               |
|--------|---------------------------|-----------------------------------------------|
| M2-T01 | Valid HELLO               | Gateway decodes and validates frame           |
| M2-T02 | Valid HELLO response      | Gateway sends ACK                             |
| M2-T03 | Matching ACK              | Client accepts echoed source/boot/sequence    |
| M2-T04 | Wrong sequence ACK        | Client rejects ACK                            |
| M2-T05 | Wrong boot ID ACK         | Client rejects ACK                            |
| M2-T06 | Duplicate HELLO           | Gateway does not deliver twice                |
| M2-T07 | Missing ACK               | Client records timeout                        |
| M2-T08 | Wrong network ID          | Frame is rejected                             |
| M2-T09 | Malformed/truncated frame | Decoder rejects without out-of-bounds access  |
| M2-T10 | Invalid NVS role          | Device enters provisioning mode; no normal TX |

### Suggested measurement run

After basic operation is stable, run at least 200 HELLO attempts at a
fixed short-range LOS setup. Record:

- attempted HELLO count;
- gateway-received count;
- ACK transmitted count;
- client-accepted ACK count;
- timeouts;
- duplicate count;
- RSSI/SNR distribution;
- resets and driver errors.

A target of at least 99% successful ACK transactions over this
short-range bench run may be used as an initial engineering gate. It is
not a field-performance guarantee. If the target is missed, preserve the
logs and diagnose before changing multiple PHY variables.

## 5. Native unit tests

Required first-stage tests:

- valid frame encode/decode round trip;
- exact byte layout and endianness;
- minimum and maximum payload;
- zero-length payload;
- truncated header;
- truncated payload;
- extra trailing byte;
- wrong magic;
- unsupported protocol version;
- unknown message type;
- wrong header length;
- invalid reserved bits;
- wrong network ID;
- wrong destination;
- ACK echo identity;
- sequence wrap behavior;
- duplicate identity detection;
- role config validation;
- invalid/partial config rejection;
- ACK bitmap 1, 10, and 70-client boundaries when M4 is implemented.

Tests must not require Arduino or RadioLib for codec logic.

## 6. M3 — beacon and one-client TDMA

Test:

- beacon reception;
- frame ID progression;
- schedule version validation;
- client slot calculation;
- gateway RX setup before the slot;
- client TX only while synchronized;
- stale beacon/schedule expiration;
- measured TX/RX switching time;
- clock drift over increasing test duration.

Acceptance:

- one client transmits only in its assigned slot;
- no slot overlap in the one-client bench test;
- guard interval is justified by measured timing;
- missed/expired beacon prevents normal uplink.

## 7. M4 — bitmap ACK and selective retransmission

Host tests:

- bitmap sizes for 1, 10, and 70 clients;
- LSB-first mapping;
- zero and all-one bitmaps;
- unused final-byte bits;
- schedule-version mismatch;
- missing packet;
- ACK bitmap loss;
- duplicate retry;
- retry limit exhaustion.

Fault injection:

- drop DATA at gateway;
- drop ACK_BITMAP at client;
- duplicate DATA;
- delay DATA;
- reboot client between superframes.

Acceptance:

- only eligible clients retry in their assigned retry slots;
- retries preserve logical packet identity;
- gateway does not deliver duplicates twice;
- retry exhaustion is logged.

## 8. M5 — conservative ADR

Test only SF7/SF8/SF9 at BW125.

Test:

- sustained good link;
- sustained degraded link;
- intermittent loss;
- ambiguous metrics;
- proposal lost;
- accept lost;
- commit lost;
- client reboot during pending change;
- stale config version;
- effective frame boundary.

Acceptance:

- gateway is the only ADR authority;
- no mid-superframe PHY change;
- client retains old committed profile if commit is not validated;
- thresholds and hysteresis are configurable and documented as
  experimental.

## 9. M6 — synchronization loss and rejoin

Test:

- one missed beacon;
- multiple missed beacons;
- client reboot;
- gateway reboot;
- stale schedule version;
- rejoin request and grant;
- normal DATA blocked before rejoin completion.

Acceptance:

- unsynchronized clients do not send normal scheduled uplinks;
- rejoin is bounded and logged;
- gateway does not accept stale schedule traffic as current traffic.

## 10. M7 — controlled antenna evaluation

Compare antennas only under controlled conditions.

Keep constant:

- radio pair;
- PHY;
- TX power;
- distance;
- device height;
- polarization;
- orientation;
- packet size;
- test duration;
- environment/machinery state.

Record:

- packet count;
- PDR and ACK success;
- RSSI and SNR distributions;
- retries;
- latency;
- antenna and cable details;
- distance and LOS/NLOS;
- supply voltage.

Do not infer antenna gain from RSSI alone without a controlled
measurement setup. Do not state a gain value unless measured or
supported by a validated antenna model.

## 11. M8 — logical-node simulation

Run host-side scheduling and airtime simulations for 5, 10, 20, 35, and
70 logical clients.

Clearly label all results as simulation. Record assumptions for:

- packet length;
- PHY distribution;
- reporting interval;
- superframe duration;
- guard time;
- radio switching time;
- ACK length;
- retry probability;
- join traffic;
- channel utilization.

Do not describe a logical-node simulation as a physical multi-node test.

## 12. Test record requirements

Every test record must include date/time, firmware version, protocol
version, Git commit, device role/ID, hardware revision, PHY, antenna,
placement, packet count, result, metrics, command output path, and
unresolved observations.

A milestone is `PASS` only when its acceptance criteria and evidence are
recorded in `docs/en/PROGRESS.md`.

## 13. Candidate evidence and operating procedure — 2026-11-12

- `logs/test/iteration-003-native.log`: 44/44 cases pass, exit 0 (codec 18, console 10, role/config 9, sequence/timing 7). Includes explicit byte layout, all header truncations, payload 0/1/68, malformed lengths, ACK echoes, role/ID constraints, every config blob byte corrupted, parser overflow, sequence exhaustion and bounded replay behavior.
- `logs/build/iteration-004-esp32.log`: firmware build exit 0. This is compile evidence only.
- NVS serialization/validation and parser workflows pass on host; actual Preferences persistence, power-cycle loading, corrupt/incompatible NVS boot, radio initialization and RF tests: NOT RUN.
- Follow `BENCH_RUNBOOK.md`, use `tools/capture_bench.py` and copy `logs/test/RUN_RECORD_TEMPLATE.md` for every physical run. Capture helper help/syntax checks do not prove serial capture operation.
- Complete M0–M2 hardware gates before M3. Negative RF identity/duplicate injection is not claimed from the pure native tests.

Final iteration-005 evidence supersedes the earlier candidate binaries: `logs/build/iteration-005-esp32.log` exit 0 and `logs/test/iteration-005-native.log` 44/44 pass, exit 0. Driver HEADER_ERR filtering is statically aligned with RadioLib HEADER_VALID qualification; no physical CRC/header test is claimed.

## 14. Software-only production state-machine verification

User inventory correction: two radio modules, one MCU, second MCU not purchased, Moxon not fabricated; no assembled pair. Do not execute the physical runbook yet.

`logs/test/iteration-007-native.log`: **92/92 PASS**, exit 0, six suites (production apps 38, codec 18, config 13, console 10, sequence/timing 7, logger 6). `logs/build/iteration-007-esp32.log`: firmware build exit 0. Exact executed case names: `logs/test/iteration-007-scenarios.md`. These are scenario counts; instrumented line/branch coverage is NOT MEASURED.

The native target compiles actual GatewayApp/ClientApp/SerialLogger plus pure codec/tracker/config/console. Fake transport is bounded to one operation and a 96-byte captured packet per endpoint; delivery is explicit. Virtual clock and Serial adapters are native-only. No application state machine is copied into a model.

Required scenarios now exercised: HELLO/ACK loss; delayed ACK before/at/after deadline; wrong echo source/boot/sequence, sender, network/destination; duplicate ACK/HELLO and byte-identical payload replay; 44 malformed HELLO and 45 malformed ACK input variants; unexpected message per role; TX start/completion errors; three-attempt RX recovery/backoff; receive errors without deadline extension; stopped new-transaction gates; client/gateway object recreation; retained/stale/out-of-order sessions; 32-bit ACK/recovery wrap and a 64-bit absolute interval; 200 repeated transactions and separate 65,537 transactions proving both senders rotate session before reusing sequence.

M2 has no automatic retry. Replay tests inject the identical captured bytes at the transport boundary and check re-ACK without duplicate acceptance; a new client transaction after timeout has a new sequence. This does not implement/selectively schedule retransmissions or establish DATA guarantees.

The Runtime reboot dispatcher, actual esp_random, NVS/Preferences/flash, RadioDriver poll/IRQ/SPI/BUSY/hardware CRC and physical RF/USB timing are not executed by these scenarios. Host queue tests do not prove SDK calls nonblocking; open audit limits are ERR-008–010. Keep M0–M2 physical acceptance IN PROGRESS.

## 15. Independent repository identity validation — iteration 008

- `pio run -e esp32c3_supermini`: exit 0, PASS; `logs/build/iteration-008-esp32.log`.
- `pio test -e native`: exit 0, 92 passed / 0 failed / 0 skipped; exact cases in `logs/test/iteration-008-scenarios.md`. The 38 integration scenarios are the same production-state-machine scenarios, not additional RF evidence.
- Static equivalence verifies all 35 production/header/test files after only identity substitutions and numeric representation of the unchanged storage namespace. Versions, wire layout/bytes, PHY, state statements and persistence calls are preserved; `logs/build/iteration-008-behavior-equivalence.log`.
- Publication checks cover identity/credentials/private paths/device IDs, current English/Chinese README facts and relative links, staged filenames/content, dependencies and artifacts. The audit inventory was created before edits (IDENTITY_AUDIT.md); previous iterations/errors/resolutions remain as sanitized copies with original hashes disclosed as historical.
- Physical NVS durability/reboot/SPI/IRQ/RF/antenna remains NOT VERIFIED. No later MAC implementation, storage migration or new adapter is validated by this task.

## 16. Iteration 009 — actual production driver fault paths

Native executes actual RadioDriver through one RadioHardware reference. All 71 new driver cases pass alongside the unchanged 92 regressions: **163/163 PASS**, exit 0, `logs/test/iteration-009-native.log`; ESP32 build exit 0, `logs/build/iteration-009-esp32.log`. Every individual result is in `logs/test/iteration-009-scenarios.md`; required A–G coverage and priorities are mapped in RADIO_DRIVER_AUDIT.md.

Fake hardware injects fixed bytes, return codes and IRQ latch/level/flags; explicit uint32 poll times cover before/at/after deadlines and wrap. It has no driver-state model. Real adapter/library/SPI/BUSY/physical ISR timing remain untested by native; frame/application/NVS/pins/PHY/version behavior is unchanged. Successful read followed by failed cleanup preserves metrics but reports ERROR/FAULT, as before. No line/branch coverage percentage or physical result is inferred.

## 17. Iteration 010 — actual configuration-store boundary

`pio test -e native` exit 0: **219/219 PASS**, including 56 actual NvsConfigStore cases and all 163 unchanged regressions. `pio run -e esp32c3_supermini` exit 0, PASS. Full logs: `logs/test/iteration-010-native.log`, `logs/build/iteration-010-esp32.log`; every individual result `logs/test/iteration-010-scenarios.md`.

CONFIG_STORE_AUDIT.md maps required A–E load/save/lifecycle/compatibility/consistency coverage. The bounded fake injects bool/count/byte/session results only; actual store and DeviceConfig/codec execute. Golden bytes, numeric namespace/key, RO/W/RO, exact operation/end order, caller unchanged on failure, no readback after failed write and error priority are asserted. Valid-but-different readback yields nvs_verify_failed. Failed save need not imply rollback. Unreachable encode-false defenses remain source-preserved without a fabricated injection claim; no line/branch coverage percentage.

Actual Preferences/IDF/NVS, flash durability, interruption/reboot/corruption recovery and wear/retention remain physically unverified. Runtime's boot and staged activation are source-reviewed, not executed by this suite. No new command syntax, migration, protocol/PHY change or later MAC feature.
