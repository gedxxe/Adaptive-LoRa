# Versioning

## 1. Separate version domains

Do not reuse one version number for unrelated purposes.

| Identifier         | Meaning                                                |
|--------------------|--------------------------------------------------------|
| `FW_VERSION`       | Firmware release version (SemVer)                      |
| `PROTOCOL_VERSION` | On-air frame format compatibility                      |
| `config_version`   | Committed radio/config profile version                 |
| `schedule_version` | Gateway schedule-table version                         |
| `BUILD_ID`         | Short Git commit hash or reproducible build identifier |
| `boot_id`          | Runtime session identifier generated at boot           |

## 2. Firmware SemVer

Use `MAJOR.MINOR.PATCH` with pre-release suffixes during development.

Suggested milestone mapping:

| Version         | Milestone                                  |
|-----------------|--------------------------------------------|
| `0.1.0-alpha.1` | LLCC68 bring-up                            |
| `0.2.0-alpha.1` | Fixed-PHY HELLO/ACK                        |
| `0.3.0-alpha.1` | Beacon and one-client TDMA                 |
| `0.4.0-beta.1`  | ACK bitmap and selective retry             |
| `0.5.0-beta.1`  | Versioned ADR and synchronization recovery |
| `0.9.0-rc.1`    | Pilot candidate                            |
| `1.0.0`         | Stable baseline after acceptance           |

This table is a proposed release sequence, not a statement that any
milestone is complete.

## 3. Protocol version

`PROTOCOL_VERSION` starts at `1`.

Bump the protocol version for an incompatible change to:

- fixed header layout;
- field width or endianness;
- message payload encoding;
- message interpretation that prevents safe mixed-version operation.

A new firmware release does not automatically require a protocol version
bump.

## 4. Configuration and schedule versions

- `config_version` increments when a gateway-committed PHY/configuration
  profile changes.
- `schedule_version` increments when the active client-to-slot mapping
  changes.
- Both must be carried where required by the protocol.
- A client must reject stale or unknown versions rather than guessing.

## 5. Build artifacts

Use role-neutral artifact naming because one firmware image selects its
role from NVS:

`adaptive-lora-v0.1.0-alpha.1-<build_id>.bin`

If role-specific artifacts are introduced later for a documented reason,
retain the same source code and record the build flags that differ.
