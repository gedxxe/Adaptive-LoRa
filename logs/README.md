# Build, test and capture records

Start with the [current results](../docs/en/PROGRESS.md) and the
[build log](../docs/en/BUILD_MEMORY.md) for the corresponding iteration.
The bulk of this directory documents earlier development runs. A
publication bundle should cite a pinned commit and include its analysis
settings, anonymized field data and the logs needed to reproduce its claims.
Raw field captures and serial device identifiers stay in private storage.
For a source-only review of a committed revision, use
`git archive -o ../adaptive-lora-source.zip HEAD AGENTS.md CHANGELOG.md CODEX_START_PROMPT.md README.md README.zh-CN.md THIRD_PARTY.md VERSION PROTOCOL_VERSION platformio.ini include src test tools docs logs/README.md logs/test/RUN_RECORD_TEMPLATE.md`.
The full checkout retains the original timestamped build and test records.
[Third-party rights](../THIRD_PARTY.md) · [Figure sources](../docs/references/FIGURE_SOURCE_REGISTER.md)

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

Historical path placeholders such as `<WORKSPACE>`, `<PLATFORMIO_HOME>`, `<USER_HOME>`, `<PIO_PYTHON>`, `<RISCV_GXX>` and `<LOCAL_ROOT>` identify redacted values. The older `esp32c3_supermini` command belongs to a prior board. Use the [current firmware environments](../docs/en/PROGRESS.md#papers-and-comparison-runs) for Heltec.

Generated SDK sources/binaries/caches remain ignored under `.pio/`. Development records under `logs/` remain tracked; do not exclude them to hide a failed build or security issue. No full-text research PDF is included.
