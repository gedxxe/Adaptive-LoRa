# Changelog

This file records externally relevant firmware and documentation
changes. It is not a substitute for the append-only build memory.

## Unreleased

### Registered Client rotation

- Added opt-in wire v6 scheduling for up to 128 registered IDs and up to eight active Clients per 15 s frame.
- The Gateway retains per-ID ADR state across groups. A Client keeps its pending sample and shows `GROUP WAIT` until selected.
- Added production-scheduler capacity checks and an authentication airtime study. Wire v6 still carries plaintext and the default firmware remains wire v5.
- Added GPLv3 for project-authored code, updated both manuals, and corrected GitHub math rendering.

### Temporary airtime reservations for all DATA SF increases

- Release the extra airtime on failed, denied, or expired SF trials.
  The normal margin decision and the delivery recovery window use
  the same deadline and cooldown.
- Commit a trial when a valid Request echoes a matching DATA ACK from
  the trial. Later timing faults leave that confirmation intact.

### Per-Client receive evidence for SF recovery

- Keep an observed primary DATA miss available to the SF search when a
  later retry or ACK has a timing fault. A local error in the primary RX
  window disqualifies that window.
- Start a budgeted higher-SF trial using the configured rolling miss
  window, so an occasional delivery can coexist with signs of degradation.

### Bounded DATA SF recovery

- After persistent complete DATA misses, try higher SFs within the
  existing airtime budget and require an ACK receipt to keep a trial result.
- After a control gap, allow a same-session Client to retry a higher SF
  at which DATA was accepted before its ACK receipt went missing.
- Keep shared control at SF9 and preserve the 15 s adaptive schedule.

### Gateway ADR hotfix

- Qualify complete DATA loss with the current Request margin in all three
  ADR rules. This avoids increasing SF solely because DATA was absent
  after a strong Request.
- Bound downshift trials by frame ID, even when reports are absent or
  excluded. Keep the previous reservation for rollback.
- Check reachability and hysteresis of policy settings at build time,
  separate model decode and policy thresholds, and update both guides and
  the LaTeX algorithms.

### Experimental Gateway ADR rules

- Add opt-in EWMA and local-trend Kalman rules that use Request SNR and
  distinguish first-attempt success, retry rescue and complete loss.
- Keep the delivery-window rule as the default with wire version 5 and the existing
  full-roster airtime check. Add per-report diagnostics and a seeded host model.

### 32-byte adaptive test samples

- Generate neutral ASCII DATA with a node ID, sample counter and `HELLO WORLD`.
- Reserve 32 B per Client in the adaptive roster and keep the 15 s frame.
- Document the eight-Client airtime limits and the effect on a possible
  70-Client rotation.

### Adaptive recovery during a walk

- Keep the last confirmed recovery SF after releasing an idle Client's airtime reservation.
- Raise the requested SF after two degraded results in three eligible reports. Roll back a lower-SF trial if the Client never confirms its receipt.
- Show missing Grants and locally missed transmissions on the Client screen.
- Update both READMEs and the LaTeX algorithms for SF recovery, rollback and airtime checks.

### SF10 DATA grants

- The adaptive image uses wire version 5 and permits SF7–SF10 DATA grants when the full roster fits. Control stays at SF9.
- The Gateway can probe back down from SF10 after sustained delivery and sufficient SNR. SF11 and SF12 remain model inputs.
- PlatformIO defaults to the adaptive image. Upgrade all boards in the roster together.

### Field display and matched SF comparison

- Every adaptive display page shows the last DATA TX or RX SF and configured CR.
- The Gateway excludes a Client's unobserved receive slot from ADR training.
- A build-time Gateway comparison holds DATA at SF7 using the same handshake and retry rules.

### Eight-Client adaptive test profile

- Reserve 16 payload bytes per Client in the adaptive test roster. The
  protocol still encodes up to 64 B, and the fixed-SF roster keeps its ceiling.
- Require on-time ACK completion for retry authorization and preserve
  Gateway delivery samples when local frame timing fails. Exclude those
  samples from ADR review.
- Capture a Gateway and up to eight Client serial ports in a single run.
  Document the modeled SF9 capacity and the 70-Client research target.

### Adaptive SF walk-test image

- Add the opt-in v4 image with SF9 control, per-Client SF7–SF9 DATA
  grants, bounded trial and rollback, and reservations for peer traffic.
- Record applied and acknowledged DATA SF, delivery observations and
  retune-to-RX service timing for a two- or five-board walk test.
- Put the adaptive setup, original vector figures and airtime equations
  in both READMEs.

### Multiclient timing and link adaptation research

- Make `heltec_wifi_lora_32_V3_multiclient` the default development image.
- Anchor v3 slots to radio completion and preserve current-time launch deadlines.
- Record per-client uplink evidence and add a configurable offline ADR evaluator.

### Protocol v3 OLED diagnostics — 2026-11-20

- Keep Client pages turning in beacon search and show `SEARCH` on every page.
  Preserve continuous RX and resume the handshake on a valid beacon.
- Give the automatic v3 OLED calendar 250 ms of display margin, and retry
  display initialization after transient I²C faults. Correct the v3 page title
  rendering so leftover characters cannot force an unchanged row to redraw.
- Use a single v3 firmware target with OLED. Show v3 link, payload and
  aggregate ACK telemetry on three pages.
- Refresh within the protected quiet tail of an active schedule. Poll for
  radio IRQs between display steps in beacon search.
- Give a v3 client a bounded startup window to show `WAIT` before listening
  for beacons. Begin listening at the deadline or after an OLED fault.
- Rotate v3 pages in order and start the dwell time after each completed
  repaint.
- Open v3 retry slots only after the gateway confirms the corresponding
  bitmap transmission in time for the retry round.

### Multi-client scheduled research link — 2026-11-20

- Add an opt-in v3 image for up to eight scheduled clients. Gateway grants
  use actual submitted byte counts. Aggregate feedback assigns shared
  retry slots and tracks delivery independently per client.
- Add a bounded 0–64-byte application interface and one bench payload source.
  Preserve the v1 and v2 wire formats and NVS schema 1.
- Set the default reporting interval to 3,000 ms and M2 ACK timeout to
  1,000 ms. Derive v3 superframe timing independently of both values.
- Add bilingual startup instructions and document the analytical capacity
  above eight clients. Physical timing and RF acceptance remain open.

### One-client scheduled research profile — 2026-11-20

- Add opt-in v2 beacon synchronization, payload-aware TDMA, bitmap feedback
  and up to three identity-preserving retries. The default and OLED builds
  still use v1 HELLO/ACK.
- Restore the 10,000 ms default reporting interval and M2 ACK timeout. Keep
  existing NVS values and schema 1. Update OLED freshness to use the ACK window.
- Request 22 dBm at fixed 915.0 MHz/SF7/BW125 and explicitly configure SX1262
  high-power protection. Conducted output, supply behavior and RF timing
  remain unmeasured.

### V3.4 bench preparation and OLED diagnostics — 2026-11-19

- Document the reported V3.4 pair and supplied monopoles, with the V3.2
  schematic and V3 Arduino variant identified as reference evidence.
- Add opt-in SSD1306 provisioning, link and counter pages with bounded
  cooperative transfers. The default build omits OLED I/O.
- Expose Gateway/Client telemetry while preserving HELLO/ACK, NVS schema 1
  and the fixed PHY. Document the authorized 3 m bench procedure and
  OLED-on/OLED-off comparison. Physical acceptance remains open.

### Pre-hardware stabilization and host-only research — 2026-11-18

- Synchronize the current agent contract and bilingual status; archive the old
  task contract and correct the active architecture label/boot order.
- Preserve all 302 native regressions and add five tracker-capacity/restart
  characterizations. Production source, NVS, PHY, protocol and versions are unchanged.
- Add tested host-only airtime/reservation tooling, 216 analytical scenarios and
  current-document consistency checks. No production TDMA/bitmap/retry/ADR is enabled.
- Keep failed checker evidence and open dependency/physical gates. No board, NVS
  or RF test is claimed; the dedicated branch is submitted for review without merge.

### Heltec V3.2 HF hardware migration - 2026-11-17

- Replace the active hardware target with ESP32-S3/SX1262 on Heltec WiFi LoRa 32 V3.2 HF: schematic pins,1.8 V TCXO, DC-DC and CP2102/UART0. Preserve one image, NVS roles, protocol1 and firmware0.2.0-alpha.1.
- Retain all290 native cases and add12 tests of the actual adapter;13 capture tests remain separate. The initial native fixture failure and correction are retained. S3 build passes.
- Publish revision/GPIO/API/resource/test/risk audit and updated bilingual bench procedures. Keep all previous iteration evidence; no physical hardware test, later MAC feature or ERR-021 recovery fix is claimed.

### Iteration 013 main integration - 2026-11-13

- Record the canonical-main workflow and fresh ESP32/290-native/13-capture validation for PR #3; preserve iteration histories and branches. No firmware, NVS, PHY, protocol or version change relative to verified Iteration 013. Physical acceptance remains unverified.

### M0-M2 physical acceptance readiness - 2026-11-13

- Prepared backed-up MCU-only and gated two-device procedures, verified vendor pin-function documentation and recorded physical wiring/USB/RF uncertainties.
- Fixed host capture interrupted-success behavior; added single-board mode, timestamp/offset/hash/fragment metadata and13 fake-port checks. Defined explicit CSV cohorts/denominators without changing firmware logs or protocol.
- Documented ERR-021 watchdog/bounded-path options and SDK startup NVS formatting risk. Firmware/bin and290 native cases unchanged; required builds/tests pass. No physical test or hardware acceptance claim.

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
