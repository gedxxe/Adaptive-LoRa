# Iteration 010 — configuration-store boundary audit

## Pre-edit baseline

Audit recorded before production source changes on 2026-11-12. Fetch completed; local and remote `iteration-009-radio-driver-verification` HEAD are exactly `3176e766d3876c86253e7694a7694560be72d1bf`, with a clean working tree. Remote main is `2a7a68120552a8037d796cf61b0af11e91abd0f2`, a subsequent merge of Iteration 009. This task branches directly from the requested Iteration 009 remote HEAD, not main. Target `iteration-010-config-store-verification` did not exist remotely. No merge/PR is part of this task.

Inspected the canonical records and current NvsConfigStore, DeviceConfig, FirmwareRuntime, provisioning parser, project constants/native filters and existing tests. Firmware 0.2.0-alpha.1, protocol 1; previous 163 native cases include no actual store execution. No dependency upgrade or hardware operation is planned.

## Existing policy, lifecycle and error order

| Path | Execution / result |
|---|---|
| load open | Local Preferences, begin original numeric namespace read-only; false -> nvs_missing_or_unreadable, no explicit end |
| load length | getBytesLength(device); anything other than 24, including missing key/zero -> end then nvs_missing_or_length; no read |
| load read/decode | getBytes(device, local 24-byte zeroed buffer); end before checking count; count !=24 -> nvs_read_failed; otherwise return actual decodeConfig result |
| save preflight | validateConfig first; invalid input returns its exact error without opening storage; encode into local 24 bytes; encode failure -> config_encode_failed |
| save write | begin namespace writable; false -> nvs_open_failed; putBytes(device,24); end before checking count; count !=24 -> nvs_write_failed, no readback |
| save readback | call actual load into a separate local config, reopening read-only after write end; load error propagated unchanged; re-encode and compare all 24 bytes; mismatch/encode failure -> nvs_verify_failed; nullptr only on full match |

Every successful open has an explicit end, including length/read/write failures. Preferences destructor also calls end, but end is a no-op after explicit closure or failed initial begin. Sharing one adapter is equivalent for existing serialized, non-reentrant usage because write closes before load verification opens. Reference lifetime and construction order must preserve that exclusivity.

Failed load preserves the caller's configuration through decodeConfig's local candidate. Failed save does not establish rollback: a write/commit/readback error may occur after data has changed. Runtime loads a fresh UNSET config and returns into provisioning before radio.begin on any load error. Saving staged console data does not mutate loaded config or switch app role; existing reboot workflow activates changes. Runtime dispatch is reviewed, not executed by this suite.

## Compatibility and reachable gaps

- Retain the original namespace numeric initializer byte-for-byte in NvsConfigStore; do not decode its historical label. Retain key `device` on all operations.
- Schema 1: exactly 24 bytes; LE magic 0x4344464D at 0, schema/u16 at 4, role/u8 at 6, zero reserved at 7, network/node/gateway u16 at 8/10/12, zero reserved u16 at 14, interval/u32 at 16, FNV-1a/u32 at 20. Hash covers bytes 0..19, seed 2166136261, multiplier 16777619, modulo 2^32; corruption detection, not authentication.
- DeviceConfig validation/encoding/decoding and all errors stay unchanged: config_corrupt, unsupported_schema, invalid_role, invalid_network_id, invalid_node_id, invalid_gateway_id, gateway_id_mismatch, client_id_conflict, invalid_hello_interval. Store prefixes/classifications above are preserved.
- Existing 13 role/config cases test pure serialization and validation, not storage operation ordering, modes, cleanup, count failures or save verification. Actual NvsConfigStore is excluded from native source filter because it instantiates Preferences.
- config_encode_failed and readback encode-false are defensive branches unreachable under the current deterministic validator/codec with fixed valid 24-byte local buffers. Do not replace the codec or add production fault hooks just to claim those branches were injected. Valid-but-different readback exercises nvs_verify_failed through actual byte comparison.

## Installed dependency inspection

Arduino-ESP32 3.3.11: inspected `libraries/Preferences/src/Preferences.h/.cpp`; framework libs 5.5.5+sha.b774170ff46: inspected ESP32-C3 `include/nvs_flash/include/nvs.h`.

- begin rejects an already-started object; otherwise calls nvs_open on default partition (read-only or writable), sets started only on success. No partition argument is supplied by current firmware.
- getBytesLength calls nvs_get_blob with null destination, returns zero on unavailable/invalid/missing/type/other error; no underlying error code is exposed.
- getBytes queries length again; invalid arguments/length or NVS error return a byte count/zero as implemented, not raw esp_err_t. This boundary does not claim concurrent-writer safety.
- putBytes calls nvs_set_blob followed by nvs_commit; either error returns zero, success returns requested length. Native injected short counts are robustness stimuli, not a claim that Preferences normally returns partial successful writes.
- end closes an active handle and clears started; closing is not a substitute for commit. The installed NVS API documents explicit commit and close responsibilities and possible storage errors. Internal IDF .cpp implementation is not supplied by the installed precompiled framework package; no physical flash/atomicity/durability behavior is inferred from headers or fake results.

## Minimal proposed boundary — before source edits

Add five-method ConfigStorage (begin, getBytesLength, getBytes, putBytes, end), held by non-owning reference in NvsConfigStore. PreferencesConfigStorage solely owns one Preferences instance and forwards these operations without validation/serialization/readback policy. Namespace/key arguments remain in existing store calls; its numeric initializer stays identical. Runtime constructs exactly one adapter before its store. The main/runtime implementation, app/radio/PHY/protocol, config model and console syntax remain untouched.

Native compiles actual nvs_config_store.cpp with actual DeviceConfig/codec. Bounded FakeConfigStorage supplies session outcomes, length/count results, bytes and ordered operation traces only; no config validation/encoding/load/save model. Tests assert modes, exact namespace/key/blob, no end after failed begin, no readback after failed write, and error priority/output preservation for all required A–E categories. One existing comment about key-level atomic updates will be clarified to avoid treating host readback as power-loss evidence; no behavior changes are intended.

Real Preferences/NVS/flash commit, power interruption, reboot recovery, corruption on hardware, wear/retention and all RF gates remain NOT VERIFIED. M0–M2 physical acceptance remains IN PROGRESS. Executed results will be appended only after commands run.

## Executed results and coverage

- `pio run -e esp32c3_supermini`: exit 0, PASS, 146.83 s. Full output `logs/build/iteration-010-esp32.log`.
- `pio test -e native`: exit 0, **219/219 PASS**, zero failed/skipped, 28.62 s. Full output `logs/test/iteration-010-native.log`.
- New actual store suite: **56/56 PASS**. Existing **163/163** ordered case names/results exactly match iteration 009 (including unchanged driver 71, app integration 38, codec 18, config 13, console 10, sequence/timing 7, logger 6).
- Every individual result: `logs/test/iteration-010-scenarios.md`; structured result `logs/build/iteration-010-validation.json`. Public logs retain complete command output except disclosed path/EOL/trailing-display formatting; exact raw outputs remain private. No new compiler warning message; ERR-005 USB CDC warning retained.

| Required category | Executed assertions |
|---|---|
| A. Load (26 cases) | Client/gateway success; failed open; missing/empty; every undersize 1..23, oversize 25/32/SIZE_MAX; every short read 0..23 and overreported count; magic/hash/schema/role/ID/interval/reserved-byte rejection; every blob byte corrupted; caller sentinel preserved for all failure fixtures; error priority |
| B. Save (24 cases) | Exact client golden blob, gateway success, all validator errors before access, writable open failure, every short write 0..23/overreported count, no readback after failure, post-mutation failure, readback open/length/read/corruption/schema/role/IDs/reserved/interval errors, valid-but-different readback, input immutable, no false success after changed backing data |
| C. Lifecycle (cross-cutting) | Ordered read-only/writable/read-only sessions, numeric namespace and device key on every recorded operation, exact 24-byte read/write requests, successful begin/end pairing, no end after failed begin, cleanup on failures, no nested session |
| D. Compatibility (cross-cutting) | Independent literal 24-byte client fixture; exact namespace numeric initializer/observed bytes, key, schema/LE/reserved/FNV; actual config codec and unchanged validation/error strings/order; no DeviceConfig source change |
| E. Consistency (6 cases) | Save/load client and gateway, updated IDs/max network+node/interval, invalid input preserves existing bytes, failed then successful sessions, three role-update/save/load cycles with nine balanced opens/closes |

Count/byte stimuli model API responses, not physical NVS. Short/overreported counts are robustness tests even where real Preferences normally returns zero or full count. The fake uses fixed arrays (32-byte storage/read payloads, 16 session plans, 64 trace entries), contains no config/store policy and asserts against illegal mode/order/buffer use. Fixture-only checksum generation allows semantic-invalid but integrity-valid input; the backend itself has no checksum or codec logic. Assertions were not weakened; all tests passed on the first run.

## Preservation review and remaining limits

`logs/build/iteration-010-source-review.json` confirms exact store-body equivalence after only Preferences local removal/reference forwarding and one clarified durability comment. Numeric namespace expression, key/error strings and ordering, all count checks, local buffers, close placement, validation/codec/readback calls remain unchanged. All 39 protected original source/header/test/version files are byte-identical, including DeviceConfig, provisioning parser, main.cpp (15 lines), runtime.cpp, all driver/app sources/tests, PHY/pins and versions. Runtime header only constructs adapter before store; native filter adds one production .cpp, firmware environment unchanged. All 48 source/test/config/version hashes match the tested snapshot.

Single persistent Preferences ownership replaces ended local objects. Current functions explicitly close every successful session before returning/reopening, so the adapter remains reusable under the same serialized non-reentrant call contract. No new guard, migration, raw-IDF error mapping or recovery policy was added. No reproduced production defect required a behavior fix. No explicit heap allocation introduced; existing SDK allocation/commit latency is not modeled.

Build RAM 24,532 bytes (unchanged from iteration 009), flash 363,854 bytes (+124). Binary layout/timing identity is not claimed. Two defensive encode-false branches remain unexercised as explained above; instrumented line/branch percentages and sanitizers NOT MEASURED.

Verified software: actual store/config/codec behavior under the listed responses, preserved source policy, and ESP32 adapter/API compilation. Simulated: low-level session success/failure/counts/bytes, backing mutation and ordered traces. Physically unverified: actual Preferences/NVS and raw flash, commit durability, power interruption, reboot/config recovery, hardware corruption, wear/retention; Runtime/entropy/USB/SPI/IRQ/RF/antenna and dependency timing limits remain open. No physical validation was performed; M0–M2 overall IN PROGRESS, later milestones NOT STARTED. Recommended next iteration: focused runtime boot/provisioning/reboot dispatch verification, preserving current role and TX fail-safe behavior.
