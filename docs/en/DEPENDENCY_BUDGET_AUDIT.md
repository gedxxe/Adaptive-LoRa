# Iteration 012 — Boot-ID liveness, dependency timing and memory budget

## Pre-edit findings and proposal

Recorded 2026-11-12 22:26 WIB before production edits. Clean local/fetched011 HEAD8fa0a473b6cc0905ddff53e4b9a5f22806e058e2;012 absent, branch created directly from requiredSHA. main5bc607cd1a86e5d86ff0fc95ba7a3cd31159b861 is not baseline. Firmware0.2.0-alpha.1/protocol1/RadioLib7.8.1;266 native baseline. Read current AGENTS, canonical architecture/runtime audit/error/decision/progress/build/test records and actual runtime/composition/apps/sequence/driver/adapter/logger/store/fakes. Installed ESP-IDF header5.5.5, Arduino3.3.11, RadioLib7.8.1; actual package/compiler probes in iteration-012-toolchain.log.

freshBootId rejection loop has no bound. Apps call it at begin and after65536 sequences. SequenceCounter.take rejects boot0, but beginning a failed zero ID would lose the previous session and allow reuse on recovery. Proposed at most8 raw calls, return0 as documented failure sentinel using existing signature. Counter renewal must reject0/previous while preserving prior exhausted state on failure. Apps log SESSION_ERROR, suppress normal TX without valid identity and retry at most once per1s using same previous ID. No predictable fallback or new NVS writes/schema. Successful flow/wire format unchanged. Finite host guard will reproduce before correction.

Runtime captures micros once before poll/app/input/save/stop/drain. Delayed RX read could pass an expired ACK using stale time; delayed save/stop could consume the100ms reset interval. If reproduced, minimally refresh after poll, arm reset after stop and check current time after drain. Preserve app-before-input, one stop, failure-still-reset/no-log-empty gate. Direct app/driver operations retain caller observation-time semantics; synchronous operations remain non-preemptible.

RadioLib Module.SPItransferStream pre/post BUSY waits each use configured1000ms timeout, allocating two scratch arrays per successful transfer without local null checks. SX126x.launchMode TX (inherited by LLCC68) has a separate BUSY ramp loop without explicit timeout. Driver2s deadline cannot interrupt a hung dependency. USB HWCDC available/write use timeout mutexes; write has ringbuffer waits/backpressure caps. Preferences putBytes calls nvs_set_blob and nvs_commit; installed IDF commit is effectively a no-op after validation, flash work occurs in set_blob/writeItem under blocking locks. Source findings do not prove actual physical faults or timing.

STATIC_ONLY defaults0. Its option makes automatic stack-local arrays, not global no-allocation; default SPI array268 bytes each. Changing it risks stack/capacity mismatch and leaves ArduinoHal/SDK allocation. Keep flags and third-party source unchanged. Baseline staticRAM24548/flash363842 is not peak memory. Logger16x512 dominates persistence; stack includes logger512+192+64+24, runtime320 and frame temporaries. ELF/allocation inventory follows. No physical validation:2LLCC68/1MCU, secondMCU/Moxon/assembly pending.

## Before-fix reproduction evidence

Finite baseline probe uses git-show exact011 runtime.cpp with existing dependencies and native raw-RNG guard64; zero/previous/alternating cases all fail to return before guard, probe exit0. Initial10 actual-runtime regression cases:9FAIL/1PASS, command exit1, iteration-012-reproduction.log. One wrap fixture also triggered an unrelated dueClient HELLO; corrected toGateway before the second reproduction, no production change. Tests show expiredACK acceptance, wrong ACK-window anchor, premature post-stop reset and delayed final reset checks. Timing stimuli are controlled host values, not physical measurements. First GDB formatted probe failed Windows quoting; plain p sizeof retry retains labels/order and actual exit separately. No production source edited yet.

Second pre-fix reproduction (Gateway wrap fixture) again9FAIL/1PASS, exit1, iteration-012-reproduction-retry.log. All nine target failures now isolate the documented source defects. Applied proposed8-attempt/zero-sentinel counter/app recovery correction and three runtime timestamp refresh boundaries; no other policy proposed. Verification pending.

## Verified correction and recovery contract

Production freshBootId makes at most8 esp_random calls per request and returns0 if all invalid. This is a call-count bound assuming each SDK call returns, not a measured microsecond bound. The existing uint32(previous) signature is sufficient because wire identity0 is already invalid. No constant/time/MAC/hash fallback is used. SequenceCounter.renew calls the supplied generator once, rejects0/the stored previous ID and leaves both boot and consumed sequence count unchanged on failure. Only success resets sequence to0. Both apps use renewal at startup/rollover, log SESSION_ERROR/boot_id_unavailable and retry on an attempt-start-based1s wrap-safe deadline. Failure inhibits all normal TX, including Gateway ACK; listening and serial help/status/show/save/reboot remain serviceable. Reboot-pending suppresses recovery attempts. Successful recovery logs SESSION_NEW/identity_recovered; failedHELLO can be replayed and re-ACKed with fresh gateway identity, preserving dedup state. No persistent writes per packet/session were introduced.

Runtime now passes a post-poll micros sample to the active app, arms rebootAt after stop completes and checks current micros after logger drain. With zero-delay dependencies existing behavior is preserved. A matching ACK finishing read at/after deadline is rejected, and ACK window begins from observed post-poll TX_DONE processing. This does not timestamp the physical IRQ. Stop failure still allows reset after the full post-stop100ms; diagnostics still need not be empty. Same-loop app-before-input behavior remains. Synchronous work can still delay service; no operation is preempted.

## Entropy assessment (source, not measurement)

The requested [stable ESP32-C3 RNG guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/random.html) resolved to6.1 during inspection; use the [installed-version5.5.5 guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/system/random.html) for this build. The bootloader enables SAR entropy temporarily, then disables it before application execution. Without RF or re-enabled SAR entropy, application startup and later rotation must be treated as pseudo-random. C3 secondary oscillator mixing does not provide the documented main-source guarantee. bootloader_random_enable touches SAR ADC2/clock/reference configuration; disable before ADC/RF use, with exclusive access. This task enables neither RF nor SAR entropy. Installed hw_random.c also busy-waits on CPU cycles between reads. No entropy-quality measurement was performed.

Valid nonzero/different-from-previous values, finite rejection and entropy quality are separate properties. A32-bit identifier can collide across power cycles or with older sessions; only the immediately previous in-RAM ID is rejected. It is not authentication/security. Future design may consider a versioned persistent boot counter, requiring atomicity/power-loss/corruption/wear/wrap/reset policy before TX. NVS schema/namespace/key remain unchanged here; no uniqueness guarantee is claimed.

## Dependency timing inventory

All rows: physical duration **NOT MEASURED**. Synchronous means the caller waits; an asynchronous RF operation may still require synchronous start/cleanup commands. Installed source hashes: logs/build/iteration-012-dependency-sources.json. Paths below are package-relative; version probes in iteration-012-toolchain.log.

| Operation/call site | Classification and source-derived waits | Deadline exposure and evidence |
|---|---|---|
| Llcc68Hardware construction | Synchronous one-time Module new ArduinoHal; no GPIO/SPI begin in constructor | Before runtime config; allocation failure is not a clean radio status. RadioLib Module.cpp:10–16, ArduinoHal.cpp constructors |
| LLCC68 begin, SPI.begin | Synchronous SPI bus/mutex/pin setup, chip reset/config/calibration; reset verification checks1000ms after nested standby calls, not a strict whole-call cap | Blocks setup; SPI.begin bool not separately checked by existing adapter. SX126x.cpp:179–211/modSetup; SPI.cpp:66 onward |
| Every SPI command | Synchronous transaction lock and byte transfer. Module pre/post BUSY waits each check spiConfig.timeout1000ms and yield; no-BUSY-pin branch delays50/1ms (not configured) | Multiple commands accumulate waits; scheduler/lock/allocator time adds. Module.h:216–224, Module.cpp:330–469; ArduinoHal.cpp:102–112. SPI_PARAM_LOCK uses portMAX_DELAY when enabled |
| TX start: Driver.transmit -> standby/startTransmit | Synchronous preparation/writeBuffer/IRQ/setTx, then asynchronous RF. PhysicalLayer.cpp:160–171 calls SX126x.stageMode/launchMode | launchMode TX ramp BUSY while at SX126x.cpp:1121–1123 has no explicit timeout. SetTx preceding command has its own checks; a later stuck/rising BUSY can still block. Driver2s deadline cannot run while blocked. Source risk, not injected hardware failure |
| TX completion: Driver.poll -> finishTransmit | Synchronous IRQ get/clear and standby; firmware uses poll (not RadioLib blocking transmit loop) | Poll entry compares caller time to2s TX deadline; a signalled TX_DONE observed before deadline may complete cleanup after it. App window now starts at post-poll time; actual RF completion time unknown. SX126x.cpp:452–458 |
| RX start: Driver.receive | Synchronous stage/config/IRQ/setRx; continuous RF RX asynchronous | Driver RX no software duration limit; BUSY/SPI still synchronous. PhysicalLayer.cpp:135–147; SX126x.stageMode/launchMode RX |
| RX completion: Driver.poll | Synchronous standby, length, readData, RSSI/SNR, finishReceive | Fresh app time rejects a late-read ACK. readData repeats status/IRQ/length/read/clear; may allocate/wait multiple times. SX126x.cpp:537–574/461–472 |
| Standby/stop | Synchronous NOP wakeup then SET_STANDBY; command checks apply | Runtime reset interval now starts after stop; failed stop still permits reset. SX126x_commands.cpp:67–101 |
| ISR/IRQ handling | Own ISR only latches volatile bool; poll samples latch or GPIO, clears latch and reads raw flags synchronously | Physical edge/clear races still unmeasured; several RadioLib getters expose no error return. No ISR SPI/allocation/logging added |
| Storage open/length/read/close | Synchronous Preferences -> NVS lock/open handle, nvs_get_blob length/content, close | Open/read may wait on locks/flash/cache; no firmware timeout. Preferences.cpp:30–59/532–559, nvs_api.cpp:268 onward, nvs_platform.cpp:19–29 |
| Storage write/commit | Synchronous nvs_set_blob -> writeItem/writeMultiPageBlob, potential page movement/erase; then nvs_commit under lock | Flash work happens during set_blob, not deferred by end. Installed NVSHandleSimple.commit:101–105 validates and returnsESP_OK; this is version-specific. No real commit latency inferred |
| Runtime config save/readback | Synchronous validation/encode, writable session/write/close, new read-only load, exact compare | Only up to64 serial input bytes, but several saves can fit and each blocks. Readback proves returned bytes, not interruption safety. NvsConfigStore.cpp unchanged |
| USB availableForWrite | Synchronous HWCDC mutex, default tx_timeout_ms100 | Capacity check can wait even before write; capacity is not a reservation. HWCDC.cpp:105/528–537 |
| USB write/drain | HWCDC mutex plus ringbuffer sends; connected branch cap20 consecutive failed sends, each default100ms; successes reset counter. Disconnected FIFO replacement is different policy | These source waits are not a measured total bound; SDK scheduling/other writers/disconnect paths matter. Logger caps submitted bytes128, not elapsed time. HWCDC.cpp:540–622; logger.cpp:drain |
| Runtime loop | poll with entry time; app with refreshed time; <=64 parser bytes; stop; <=128 logger bytes; current reset check | App action before input is retained. Save/serial may defer ACK/recovery until next loop. Fresh samples correct known staleness, not all latency. micros wrap comparisons assume intervals/service gaps below2^31us |
| Random generation | At most8 SDK calls per request; SDK hw_random.c:esp_random cycle wait | Finite firmware retry count does not establish entropy or hard wall-clock WCET; repeated failures get1s app cooldown |

Open priorities: audit/bound the upstream TX BUSY-ramp path before claiming fault-tolerant loop latency; characterize SPI locks, flash page/erase and serial waits. No third-party code/flags changed and no new tasks/watchdogs/scheduler added. These unresolved dependency risks block a hard real-time acceptance claim, not the completed source/test assessment.

## Allocation and stack inventory

| Owner/path | Allocation frequency/budget | Failure/fragmentation disposition |
|---|---|---|
| Esp32Firmware/FirmwareRuntime | One static composition; inline store/driver/console/logger and both optional app storage, no project new/malloc in transaction path | Native fake/test strings may allocate; not a production heap result. Both optional storage areas consume RAM even when one role inactive |
| Frames/sequence | Frame sizeof92, RadioEvent120; wire max96/header28/HELLO28/ACK37, payload max68; tracker200/counter8 bytes on target | Bounded stack/local/event copies and8-session/64-sequence cache. Counter renewal has no dynamic allocation or NVS write |
| SerialLogger | Persistent8228 bytes (8192 queue+metadata); event local char arrays512+192+64+24=792 bytes before ABI/compiler/libc overhead | Full/oversized lines increment dropped count. No protocol-path String. Runtime status/config local320, Client finish256, app TX bytes96 plus Frame locals; nested formatting/SPI/libc stack not summed into a measured peak |
| NvsConfigStore | Non-owning4-byte object, save locals24+24+DeviceConfig; load nested24-byte buffer | Project store allocates no heap; backend handle/page bookkeeping can allocate |
| RadioLib Module constructor | One ArduinoHal heap object in static construction | No project allocation check/catch; constructor/heap failure not physically tested |
| Module.SPItransferStream | Two new[] scratch arrays per successful command, freed on normal/error returns; repeated during IRQ/standby/TX/RX/metrics | Payload write scratch: len+2 each (HELLO30, ACK39, max98). Payload read: len+3 each (max99), hence198 bytes simultaneously for max RX scratch alone, excluding allocator/HAL/SDK overhead. Register/config operations have separate small commands; this is not a whole-library peak budget |
| RadioLib allocation failure | Throwing new[] results are not checked/caught in this path | No recoverable RADIO_ERROR/OOM guarantee; allocator/new-handler/exception termination path not injected. Fragmentation/peak and long-run heap behavior remain unmeasured |
| STATIC_ONLY alternative | Defaults0, STATIC_ARRAY_SIZE256, STATIC_SPI_ARRAY_SIZE268. Enabling gives two automatic268-byte arrays per Module stream transfer (536 stack), not persistent static pools | Requires packet/command capacity and full nested stack audit; does not remove ArduinoHal or SDK allocations. Kept disabled; library untouched |
| Arduino/IDF startup/services | SPI mutex/bus state, USB mutex/RX queue/TX ring default256 each, IRQ/event service, FreeRTOS task stacks; NVS handles/namespace/blob/page nodes | SDK checks some allocation failures (e.g.NVS ESP_ERR_NO_MEM, USB size0), others only log/terminate. Project has no full OOM recovery policy. Repeated saves open/close backend handles; not per packet |
| Task stacks | Installed C3 config Arduino loop8192, IDF main4096; configured allocations, not measured headroom | FreeRTOS allocation/other SDK service stack/ISR/formatting needs included separately from compiler data+bss. Physical high-water/peak heap/largest free block **NOT MEASURED** |

No new explicit production allocation or growing collection is added. Repeated transaction tests run actual bounded state/code and clear captured host logs, but cannot measure ESP32 allocator fragmentation, radio scratch allocation count, SDK memory peaks or physical stack margins.

## ELF/static budget

GDB batch p sizeof queried the actual target ELF without connecting a device. Initial Windows printf quoting failure is retained, corrected baseline/final logs identify ordered type names. Section dump via riscv32-esp-elf-size -A and full symbol-size dump via nm -S --size-sort -C are retained in iteration-012-elf-size.log/elf-nm.log. Object sizes overlap containment; do not sum table rows as separate allocations.

| Target object | Baseline011 bytes | Final012 bytes |
|---|---:|---:|
| FirmwareRuntime |8984|9000|
| Esp32Firmware |9552|9568|
| GatewayApp |344|352|
| ClientApp |184|192|
| RadioDriver |20|20|
| Llcc68Hardware |552|552|
| NvsConfigStore |4|4|
| PreferencesConfigStorage |12|12|
| SerialLogger |8228|8228|
| Frame / RadioEvent |92 /120|92 /120|
| SequenceTracker /Counter |200 /8|200 /8|

Compiler data+bss RAM24548 ->24564 (**+16**) /327680. Compiler flash363842 ->364308 (**+466**) /1310720. Final sections: .iram0.text58428, .flash.text226000 (baseline225590), .dram0.data7980 unchanged, .dram0.bss16584 (baseline16568), .flash.rodata71900 (baseline71844). Symbol runtime is0x2560=9568 bytes. .eh_frame25400 and other image sections are separate: build reports total image390256 bytes; compiler flash summary is not the padded/full binary size. Debug and dummy alias/mapping sections are not additional payload RAM/flash to simply add. Binary SHA-256 in iteration-012-validation.json. Peak heap, stack high-water and latency **NOT MEASURED**.

## Executed test evidence and compatibility

`pio test -e native` exit0, **290/290 PASS**,77.575s (runner80.660s): all266 old ordered case results and cpp bytes unchanged plus24 new cases. `pio run -e esp32c3_supermini` exit0, PASS31.748s (runner32.399s), incremental build on installed resolved packages. Only existing ERR-005 CDC warning message (one occurrence); no native warnings. Source53-input manifest matches frozen tested tree; protected39 files byte-identical. Initial focused RED outputs9FAIL/1PASS both retained, corrected initial10/10 retained. Every final case/name/result in logs/test/iteration-012-scenarios.md; no line/branch coverage percentage claimed.

24 new cases cover initial Client/Gateway failed RNG, valid maximum/zero prefix, last8th attempt and deferred9th value,1s rate/recovery, reboot suppression, retry-wrap, counter identity preservation after65536 takes, Gateway three stuck patterns after65536 ACKs each, Client failure/recovery after65536 HELLOs, late ACK/read/wrap, TX_DONE-delay window, delayed save/stop/reset/drain/recovery, and unchanged start-anchored radio deadline semantics. Baseline probe instruments actual011 generator from Git and stops64 rejected calls; no infinite test executed. Scripted delays and host runtime durations are not hardware measurements.

Compatibility: firmware0.2.0-alpha.1/protocol1/RadioLib7.8.1, wire fields/layout/identity,24-byte NVS schema/numeric namespace/device key, IDs/role provisioning, pins/PHY and one image unchanged. Main unchanged15 lines; radio/store interfaces and real adapters unchanged. Intentional behavior corrections are bounded/observable identity failure/recovery and three demonstrated runtime timestamp boundaries; no later MAC or external integration. Exact changed files/responsibilities are in iteration-012-review.json.

## Hardware measurement plan and remaining gates

No upload, assembled link or physical test. Inventory2RA-01SCH/LLCC68,1ESP32-C3 Super Mini; second MCU/915MHz Moxon pending. Pins/module revision/supply/USB mode/antenna/legal RF setup remain unverified. Distance/LOS/orientation/supply/PDR/RSSI/SNR/latency/reset counts **NOT MEASURED**. M0–M2 physical gates IN PROGRESS; M3–M8 NOT STARTED.

After assembly: capture GPIO/logic-analyzer timestamps around SPI/BUSY, start/finishTX/RX, poll/app/serial/save/stop/reset with exact PHY/supply and IRQ traces; include fault BUSY/DIO levels and USB host-stalled/disconnected conditions. Measure flash save/readback under page pressure and controlled reset/power interruption; retain raw logs. Record free/minimum/largest heap block and task high-water before/after repeated transactions/config saves, cold/warm boots and error recovery. Audit allocator failure policy separately. Verify entropy-source enable/disable compatibility before considering any source change; do not infer quality or uniqueness from raw values alone. Resolve upstream BUSY timeout/driver status limitations before any hard latency claim. No hardware performance claim follows from these tests.

Entropy terminology: the ESP-IDF RF condition refers to the ESP32 Wi-Fi/Bluetooth subsystem, not the external LLCC68 LoRa radio. Enabling the LoRa module does not establish that entropy condition.

RAM accounting clarification: the linker report also shows DRAM82,992/321,296 bytes used, including58,428 instruction bytes plus7,980 data and16,584 bss. The PlatformIO24,564-byte RAM summary counts data+bss only. Neither reported remainder is measured allocatable/minimum heap; IRAM/DRAM aliases, SDK reservations, startup allocations and task stacks matter. Do not add dummy mapping sections again.

## Iteration013 ERR-021 disposition (source review, not a physical reproduction)

Exact path: ClientApp HELLO or GatewayApp ACK -> RadioDriver.transmit -> Llcc68Hardware.startTransmit -> inherited PhysicalLayer.startTransmit -> SX126x.stageMode(TX) -> SX126x.launchMode(TX) -> setTx(0) -> separate BUSY polling loop. It persists if the configured BUSY input stays high after setTx has returned success. High BUSY earlier can instead trigger Module's timed pre/post-command checks; those do not enclose the later ramp loop. TX_DONE/DIO1 flags, serial commands, application deadlines and radio.stop are not consumed until the call returns. The current driver cannot recover from that loop itself; manual removal of both MCU/radio power is a bench stop action, not verified automated recovery.

Installed Arduino3.3.11/IDF5.5.5 sources set loopTaskWDTEnabled=false and only subscribe it through enableLoopWDT, which this project does not call. Inspected C3 config enables/init/panic TWDT5s but has no CHECK_IDLE_TASK_CPU0 definition. ArduinoHal.yield calls Arduino yield -> vPortYield; that is not proof of application progress or idle scheduling. Tick-fed IWDT300ms protects interrupt progress, not this radio transaction. No guaranteed watchdog detection/reset is established. A deliberately subscribed loop/user progress watchdog could detect lack of feeds, subject to actual configuration/independent clock/panic/debug behavior; no such change or physical test is included. [Installed-version WDT reference](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/system/wdts.html) also warns that JTAG/OpenOCD alters watchdog behavior.

| Option | Bounded disposition and consequences |
|---|---|
| Small upstream/backported loop timeout | A finite timeout around this exact loop, returning an existing error, is a plausible localized fix. Must select a justified ramp bound, review inherited-chip effects and radio-state cleanup, pin/hash the approved dependency and regression/hardware-test normal/late/stuck BUSY. Do not edit installed library files automatically |
| Public wrapper/ordinary2s driver deadline | Cannot preempt a call already inside the library. A wrapper time check after return is insufficient |
| Adapter subclass/direct commands | Would need to preserve staging, RF-switch, sensitivity, error and private/protected state semantics; not assumed safe from current interface. Requires separate reviewed proof; no speculative override added |
| Progress watchdog/reset | Detect/reset path is distinct from a returned radio error; may lose logs/identity and does not prove radio power/PA stopped. Needs explicit configuration and measured reset/initialization behavior; current defaults are not a disposition |
| Attended bench abort | After M0, use current limiting, attached antenna/load and accessible power removal; stop on missing progress. Does not close fault-recovery or unattended acceptance |

ERR-021 remains OPEN. Unattended/stuck-BUSY recovery and any hard timing acceptance are BLOCKED until an approved bounded-path/watchdog disposition is verified. Preparation and normal attended observations may proceed under gates; M1/M2 cannot be reported fully closed by such observations alone. No firmware/library/watchdog configuration changed in013. Exact inspected hashes/config values: iteration-013-readiness-sources.json.
