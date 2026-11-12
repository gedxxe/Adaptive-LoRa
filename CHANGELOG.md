# Changelog

This file records externally relevant firmware and documentation
changes. It is not a substitute for the append-only build memory.

## Unreleased

### Runtime reliability and dependency budgets — 2026-11-12

- Fixed reproduced unbounded boot-ID rejection: eight-call bound, explicit unavailable identity, exhausted-session preservation, TX suppression and one-second observable recovery; no predictable fallback or NVS schema change.
- Fixed reproduced stale runtime timestamps after radio polling, stop and diagnostic draining. Preserved stop-failure/reset and app-before-command policies.
- Added24 reliability cases; all266 earlier tests pass unchanged (290 total), ESP32 PASS. RAM24,564 (+16), flash364,308 (+466); source/ELF timing/allocation audit records open SDK/BUSY/entropy and physical limits. Versions/wire/PHY unchanged.

### Production runtime lifecycle verification — 2026-11-12

- Injected existing storage/radio references into FirmwareRuntime; ESP32 composition owns the concrete adapters. main remains 15 lines; coordinator policy unchanged.
- Added 47 production-runtime boot/provisioning/serial/radio/reboot/session/diagnostic cases. All219 regressions remain passing (266 total); ESP32 build PASS with no new warning messages.
- Documented existing stop-failure/reset and undrained-log behavior, deterministic SDK boundaries, reference lifetimes and +16 RAM/-12 flash bytes. Versions, NVS/wire/PHY and app policy unchanged. No physical NVS/reset/USB/SPI/RF or entropy validation.

### Configuration-store boundary verification — 2026-11-12

- Added five-operation ConfigStorage reference and sole-owner PreferencesConfigStorage adapter; actual NvsConfigStore policy now runs under native fault injection.
- Added 56 passing storage cases; all 163 existing cases pass unchanged (219 total); ESP32 build passes with no new warning messages.
- Preserved numeric namespace, device key, schema-1 24-byte LE/FNV format, error priorities, cleanup, readback and staged boot behavior. Versions unchanged; no hardware durability/power-loss/rollback claim or later MAC feature.

### Production radio-driver fault verification — 2026-11-12

- Separated unchanged RadioDriver policy from LLCC68/SPI/GPIO ownership through one non-owning RadioHardware reference and Llcc68Hardware adapter; application transport unchanged.
- Added 71 deterministic production-driver fault cases; all 92 existing cases remain passing (163 total); ESP32 build passed.
- Preserved state/error/header qualification/deadline/metrics semantics, firmware 0.2.0-alpha.1, protocol 1, NVS bytes and fixed PHY. Physical adapter/IRQ/SPI/RF and dependency timing/allocation remain unverified.

### Adaptive-LoRa repository establishment — 2026-11-12

- Established independent research identity and repository metadata; industrial energy monitoring remains a general application domain.
- Neutralized 265 historical organization/site references plus workstation/device labels, preserving chronological results and error resolutions with redaction provenance and digest records.
- Mechanically renamed C++/native-test identifiers; retained on-device namespace bytes numerically for saved-config compatibility. Firmware 0.2.0-alpha.1, protocol 1, wire/PHY/state logic unchanged.
- Documented implemented versus target protocol/radio/platform/application boundaries. Added no TDMA, bitmap, selective retransmission, ADR or external integration.
- Revalidated ESP32 build PASS and native 92/92 PASS (38 integration scenarios); physical NVS/reboot/SPI/IRQ/RF/antenna remains unverified.

### Software verification of 0.2.0-alpha.1 — 2026-11-12

- Added a six-method transport seam and host tests using actual production client/gateway/logger code; firmware state statements, PHY, provisioning and protocol v1 remain unchanged.
- Fixed legacy MinGW format-macro compilation by using standard C++ `<cinttypes>` in client/logger. ESP32 build passed; 92 native tests passed, including 38 integration scenarios.
- Recorded synchronous NVS/BUSY/SDK serial limits and RadioLib SPI allocation risk; no nonblocking/hardware validation claim.
- Corrected current stock to two LLCC68 modules, one MCU and no fabricated Moxon; physical testing is deferred. No automatic retry, TDMA, bitmap, ADR or external integration added.

### 0.2.0-alpha.1 implementation candidate — 2026-11-12

- Added one-image NVS role provisioning, staged serial configuration and controlled reboot.
- Added LLCC68 asynchronous fixed-PHY HELLO/ACK, explicit bounded v1 codec, ACK echo checks and bounded duplicate tracking.
- Added configurable HELLO interval and queued parseable diagnostics.
- Added native codec/config/sequence/console tests: 44 passed; ESP32 firmware build passed. Earlier failures remain recorded. Physical NVS/radio/RF acceptance is pending.
- Added bilingual bench procedures, raw dual-port capture helper and unexecuted run-record template; all pins/module/USB assumptions remain provisional.
- Aligned receive header-error qualification with pinned RadioLib to avoid false rejection when HEADER_VALID is present; final firmware/native checks pass, physical verification pending.

- Established a single firmware image with NVS-backed Gateway/Client
  role selection.
- Defined safe provisioning behavior when the saved role is missing or
  invalid.
- Defined the first fixed-PHY HELLO/ACK implementation boundary.
- Added explicit build-memory, error-log, and architecture-decision
  record requirements.
- The original documentation baseline contained no source or RF results; the candidate above adds source. No physical RF result is claimed.

## Release rules

- Add a dated entry for every firmware release.
- Keep entries factual and tied to a firmware version or protocol
  change.
- Do not describe planned features as implemented.
