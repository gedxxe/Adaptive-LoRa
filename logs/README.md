# Raw Build and Test Logs

Save full command output here when it is useful for debugging or audit.

Suggested naming:

- `logs/build/ITERATION-001-esp32-build.txt`
- `logs/test/ITERATION-001-native-tests.txt`
- `logs/test/ITERATION-002-two-device-serial-gateway.txt`
- `logs/test/ITERATION-002-two-device-serial-client.txt`

Every log referenced from `docs/en/BUILD_MEMORY.md` or
`docs/en/ERROR_LOG.md` must exist at the referenced path.

Do not put secrets, Wi-Fi credentials, private keys, or unrelated
personal data in logs.

## Publication provenance

The archived iteration 000–007 records and review snapshots are sanitized publication copies. Organization/site labels, internal code labels, workstation paths and device-enumeration identifiers were removed or neutralized in iteration 008. Results, chronological order, available timestamps, versions, errors and resolutions were preserved; no host result was relabelled as physical evidence.

Unredacted originals were retained outside the publication tree. Their locations and private labels are not committed. `logs/build/iteration-008-sanitization-manifest.json` records original/public-copy digests and occurrence counts without exposing raw values. Historical source/artifact SHA-256 values remain fingerprints of originals; they are not assertions that redacted archive files are byte-identical. Current builds have new source/image manifests.

Historical path placeholders such as `<WORKSPACE>`, `<PLATFORMIO_HOME>`, `<USER_HOME>`, `<PIO_PYTHON>`, `<RISCV_GXX>` and `<LOCAL_ROOT>` disclose redaction, not commands actually run with those literal placeholders. Use `pio run -e esp32c3_supermini` and `pio test -e native` for current portable instructions.

Generated SDK sources/binaries/caches remain ignored under `.pio/`. Development records under `logs/` remain tracked; do not exclude them to hide a failed build or security issue. No full-text research PDF is included.
