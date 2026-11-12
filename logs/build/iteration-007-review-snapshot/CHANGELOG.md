# Changelog

This file records externally relevant firmware and documentation
changes. It is not a substitute for the append-only build memory.

## Unreleased

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
