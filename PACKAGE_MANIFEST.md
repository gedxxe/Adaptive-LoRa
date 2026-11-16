# Adaptive-LoRa package manifest

Project: Adaptive-LoRa

Research title: Reliability-aware TDMA Scheduling and Link Adaptation for Single-Radio LoRa Industrial Energy Monitoring Networks.

Firmware: 0.2.0-alpha.1; protocol: 1. English is canonical; Simplified Chinese is secondary.

## Included publication content

- One-image Heltec WiFi LoRa 32 V3.2 HF / ESP32-S3 / SX1262 firmware, configuration, role state machines and bounded protocol modules.
- Native unit/integration tests: 302/302 PASS, including all290 previous cases and12 production-adapter cases;13 capture utility tests are recorded separately.
- Engineering requirements, architecture/gap analysis, provisional hardware facts, milestone plan and deferred bench procedure.
- Sanitized build/test transcripts, chronological iteration/error/decision records and source/artifact fingerprints.
- A bounded bench-capture helper; no actual bench run is claimed.

## Provenance and limits

The original documentation-only package revision was 2.0, dated 2026-11-12 (iteration 000). It contained no firmware/build/RF evidence at that time. Later implementation records establish the current software baseline; the earlier outcomes are not rewritten as new validation.

The confirmed target is Heltec WiFi LoRa 32 V3.2 HF; delivery quantity, PCB markings and physical setup have not been inspected. The915 MHz Moxon remains the design baseline. Physical NVS/reboot/SPI/IRQ/RF/antenna behavior remains unverified.

Generated SDKs/caches/binaries, private originals, local settings, credentials and full-text research papers are not publication content. Review IDENTITY_AUDIT.md, logs/README.md and THIRD_PARTY.md for redaction and licensing provenance. No later MAC feature is implemented by the hardware migration.
