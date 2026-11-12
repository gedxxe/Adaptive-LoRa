# Adaptive-LoRa pre-edit identity/publication audit

Inventory prepared before modifying existing files. Private originals are preserved outside the publication tree; this inventory intentionally does not repeat removed identifiers. Counts are occurrence counts, not unique people or systems.

## Findings summary

- Publication candidates inspected: 253 text files (including all prior log snapshots).
- Organization/project-label matches: 261.
- Site-identifier matches: 4.
- Workstation/path findings: 110.
- Device-enumeration identifier findings: 6.
- High-confidence credential candidates: 0.
- No local Git repository, branches, remotes or commits existed. Existing global author configuration is available; its private contact details are not copied into tracked content.
- The configured public GitHub destination exists; API history/tree queries report an empty Git repository (HTTP 409), not an initial README commit. Git fetch will verify before publication.
- No PDF, archive, private-key or environment file was found among publication candidates. Generated SDK/library/binary content is confined to ignored build/cache directories; public tool/vendor URLs remain legitimate references.
- The external checkout directory has a machine-specific label; it is not a tracked filename. Ignored compiler/debug/cache files can contain that external location and are not publication content.
- Current runtime configuration storage uses a legacy namespace. Renaming its bytes would invalidate existing saved configuration; retain those bytes as a neutral numeric compatibility constant, with an explicit exception note. C++ namespace and native-test macro labels can be renamed without changing state or wire semantics.

## Per-file inventory

| File | Issue codes/counts | Action | Layer |
|---|---|---|---|
| `AGENTS.md` | ORG_IDENTITY: 4, SITE_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `CODEX_START_PROMPT.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `PACKAGE_MANIFEST.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `platformio.ini` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `README.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `README.zh-CN.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `include/board_pins.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `include/project_config.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `include/radio_config.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/main.cpp` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/support/fake_radio_transport.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/test_app_integration/test_main.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/test_frame_codec/test_main.cpp` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/test_provisioning_console/test_main.cpp` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/test_role_config/test_main.cpp` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/test_sequence_tracker/test_main.cpp` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/test_serial_logger/test_main.cpp` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/support/platform/Arduino.h` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `test/support/platform/esp_timer.h` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/app/firmware_runtime.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/app/firmware_runtime.h` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/app/nvs_config_store.cpp` | ORG_IDENTITY: 4 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/app/nvs_config_store.h` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/app/provisioning_console.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/app/provisioning_console.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/client/client_app.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/client/client_app.h` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/gateway/gateway_app.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/gateway/gateway_app.h` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/config/device_config.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/config/device_config.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/diagnostics/serial_logger.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/diagnostics/serial_logger.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/protocol/frame_codec.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/protocol/frame_codec.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/protocol/message_types.h` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/protocol/sequence_tracker.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/protocol/sequence_tracker.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/radio/radio_driver.cpp` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/radio/radio_driver.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `src/common/radio/radio_transport.h` | ORG_IDENTITY: 2 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `logs/build/iteration-001-dependencies.log` | WORKSTATION_PATH: 18 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-001-esp32.log` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-004-packages-final.log` | WORKSTATION_PATH: 4 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-004-packages.log` | WORKSTATION_PATH: 16 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-complete.diff` | ORG_IDENTITY: 20, SITE_IDENTITY: 1, WORKSTATION_PATH: 7 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/test/iteration-004-serial-ports.log` | DEVICE_ID: 6 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/test/iteration-006-native.log` | ORG_IDENTITY: 30 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/AGENTS.md` | ORG_IDENTITY: 4, SITE_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/CODEX_START_PROMPT.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/PACKAGE_MANIFEST.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/README.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/README.zh-CN.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/AGENTS.md` | ORG_IDENTITY: 4, SITE_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/CODEX_START_PROMPT.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/PACKAGE_MANIFEST.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/platformio.ini` | ORG_IDENTITY: 1 | Rename identifiers mechanically; preserve protocol/storage bytes | source/test |
| `logs/build/iteration-007-review-snapshot/README.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/README.zh-CN.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/include/board_pins.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/include/project_config.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/include/radio_config.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/support/fake_radio_transport.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/test_app_integration/test_main.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/test_frame_codec/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/test_provisioning_console/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/test_role_config/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/test_sequence_tracker/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/test_serial_logger/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/support/platform/Arduino.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/test/support/platform/esp_timer.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/app/firmware_runtime.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/app/firmware_runtime.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/app/nvs_config_store.cpp` | ORG_IDENTITY: 4 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/app/nvs_config_store.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/app/provisioning_console.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/app/provisioning_console.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/client/client_app.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/client/client_app.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/gateway/gateway_app.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/gateway/gateway_app.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/config/device_config.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/config/device_config.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/diagnostics/serial_logger.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/diagnostics/serial_logger.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/protocol/frame_codec.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/protocol/frame_codec.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/protocol/message_types.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/protocol/sequence_tracker.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/protocol/sequence_tracker.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/radio/radio_driver.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/radio/radio_driver.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/src/common/radio/radio_transport.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/en/ARCHITECTURE.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/en/BENCH_RUNBOOK.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/en/BUILD_MEMORY.md` | WORKSTATION_PATH: 14 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/en/ERROR_LOG.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/en/IMPLEMENTATION_REPORT.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/en/PROTOCOL.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/en/SOFTWARE_AUDIT.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/en/VERSIONING.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/zh-CN/ARCHITECTURE.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/zh-CN/BENCH_RUNBOOK.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-007-review-snapshot/docs/zh-CN/PROTOCOL.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/include/board_pins.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/include/project_config.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/include/radio_config.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/test/test_frame_codec/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/test/test_provisioning_console/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/test/test_role_config/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/test/test_sequence_tracker/test_main.cpp` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/app/firmware_runtime.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/app/firmware_runtime.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/app/nvs_config_store.cpp` | ORG_IDENTITY: 4 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/app/nvs_config_store.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/app/provisioning_console.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/app/provisioning_console.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/client/client_app.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/client/client_app.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/gateway/gateway_app.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/gateway/gateway_app.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/config/device_config.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/config/device_config.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/diagnostics/serial_logger.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/diagnostics/serial_logger.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/protocol/frame_codec.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/protocol/frame_codec.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/protocol/message_types.h` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/protocol/sequence_tracker.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/protocol/sequence_tracker.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/radio/radio_driver.cpp` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/src/common/radio/radio_driver.h` | ORG_IDENTITY: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/en/ARCHITECTURE.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/en/BENCH_RUNBOOK.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/en/BUILD_MEMORY.md` | WORKSTATION_PATH: 9 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/en/ERROR_LOG.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/en/IMPLEMENTATION_REPORT.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/en/PROTOCOL.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/en/VERSIONING.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/zh-CN/ARCHITECTURE.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/zh-CN/BENCH_RUNBOOK.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `logs/build/iteration-006-baseline/docs/zh-CN/PROTOCOL.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `docs/en/ARCHITECTURE.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `docs/en/BENCH_RUNBOOK.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `docs/en/BUILD_MEMORY.md` | WORKSTATION_PATH: 14 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `docs/en/ERROR_LOG.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `docs/en/IMPLEMENTATION_REPORT.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `docs/en/PROTOCOL.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `docs/en/SOFTWARE_AUDIT.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | historical record |
| `docs/en/VERSIONING.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `docs/zh-CN/ARCHITECTURE.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `docs/zh-CN/BENCH_RUNBOOK.md` | WORKSTATION_PATH: 2 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |
| `docs/zh-CN/PROTOCOL.md` | ORG_IDENTITY: 1 | Sanitize identity/path labels; preserve chronology, results, errors and hardware facts | documentation |

## Handling policy

Historical records keep their order, timestamps, test totals, tool versions and error outcomes. Identity/path redaction is the only exception to byte-for-byte append-only storage, expressly authorized by this migration; a new iteration documents it. Original artifact/source hashes remain historical fingerprints, not claims that sanitized archives are byte-identical. Publish a before/after digest manifest with no raw private values.

Publication copies also normalize line endings and incidental trailing display whitespace. These formatting changes do not alter recorded values, results or error meaning. The original private copies retain the exact earlier bytes. Public tooling/vendor references and required hardware names are legitimate technical citations, not deployment affiliation.
Hardware vendor/model/library names and their legitimate public documentation are allowed technical references. No deployment customer identity, local path, credential or personal device identifier is to be committed. No protocol feature implementation is part of this task.
