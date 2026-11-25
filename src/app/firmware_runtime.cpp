#include "firmware_runtime.h"
#if defined(ADAPTIVE_LORA_SECURE) && !defined(ADAPTIVE_LORA_NATIVE_TEST)
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include <Arduino.h>
#include <esp_system.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <cstdio>
#include <cinttypes>
#if defined(ADAPTIVE_LORA_MULTICLIENT)
#include "research_roster.h"
#endif

namespace adaptive_lora {
namespace {
#if defined(ADAPTIVE_LORA_MULTICLIENT)
constexpr auto wireVersion = multiclient::kWireVersion;
#if defined(ADAPTIVE_LORA_FIXED_SF_COMPARISON)
static_assert(multiclient::kAdaptive, "Use the adaptive environment for matched SF7 comparison");
constexpr bool adaptDataSf = false;
#else
constexpr bool adaptDataSf = multiclient::kAdaptive;
#endif
#if defined(ADAPTIVE_LORA_SECURE)
constexpr const char* profile = "secure";
#elif defined(ADAPTIVE_LORA_ROTATING)
constexpr const char* profile = "rotating";
#else
constexpr const char* profile = multiclient::kAdaptive ? "adaptive" : "multiclient";
#endif
#if defined(ADAPTIVE_LORA_OLED)
constexpr uint32_t kClientDisplayStartupUs = 400000;
constexpr uint32_t kDisplayTailReserveUs = 250000;
#endif
#if !defined(ADAPTIVE_LORA_ROTATING)
multiclient::Roster benchRoster() {
    auto roster = multiclient::kBenchRoster;
#if defined(ADAPTIVE_LORA_OLED)
    if (!roster.periodUs) {
        const auto minimum = multiclient::requiredPeriodUs(roster);
        if (minimum && minimum <= multiclient::kMaxPeriodUs - kDisplayTailReserveUs)
            roster.periodUs = minimum + kDisplayTailReserveUs;
    }
#endif
    return roster;
}
#endif
#elif defined(ADAPTIVE_LORA_SCHEDULED)
constexpr auto wireVersion = scheduled::kVersion;
constexpr const char* profile = "scheduled";
#else
constexpr auto wireVersion = kProtocolVersion;
constexpr const char* profile = "m2";
#endif
uint32_t freshBootId(uint32_t previous) {
    // Valid-value bound only; entropy quality depends on ESP32-S3 SDK setup.
    for (unsigned attempt = 0; attempt < kBootIdAttempts; ++attempt) {
        const uint32_t value = esp_random();
        if (value != 0 && value != previous) return value;
    }
    return 0;
}
}
void FirmwareRuntime::showConfig(const DeviceConfig& c, const char* scope, const char* validity) {
    char line[320];
    std::snprintf(line, sizeof(line),
        "event=CONFIG,scope=%s,role=%s,network_id=%u,node_id=%u,gateway_id=%u,schema=%u,validity=%s,hello_interval_ms=%" PRIu32,
        scope, roleName(c.role), c.networkId, c.nodeId, c.gatewayId, c.schema, validity, c.helloIntervalMs);
    log_.text(line);
}
void FirmwareRuntime::status() {
    showConfig(loaded_, loadError_ ? "inactive_defaults" : "loaded", loadError_ ? loadError_ : "valid");
    char line[320];
    std::snprintf(line, sizeof(line),
        "event=STATUS,mode=%s,radio=%s,chip=%s,fw=%s,protocol=%u,build_id=%s,reset_reason=%d,log_dropped=%" PRIu32 ",profile=%s",
        loadError_ ? "PROVISIONING" : (radio_.healthy() ? roleName(loaded_.role) : "RADIO_FAULT"),
        radio_.initialized() ? (radio_.healthy() ? "ready" : "fault") : "disabled",
        radio_.initialized() ? radio_.chip() : "N/A", kFirmwareVersion, wireVersion, kBuildId,
        static_cast<int>(esp_reset_reason()), log_.dropped(), profile);
    log_.text(line);
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    if (!loadError_ && loaded_.role == DeviceRole::GATEWAY) {
#if defined(ADAPTIVE_LORA_ROTATING)
        std::snprintf(line, sizeof(line), "event=SCHEDULE_CONFIG,period_us=%" PRIu32
                      ",registered_count=%u,max_group=%u,payload_ceiling_bytes=%u,adr_rule=%s",
                      multiclient::kAdaptivePeriodUs,
#if defined(ADAPTIVE_LORA_SECURE)
                      unsigned(security_.population()),
#else
                      unsigned(multiclient::kRegisteredCount),
#endif
                      unsigned(multiclient::kMaxClients), unsigned(multiclient::kBenchPayloadBytes),
                      adaptDataSf ? adrRuleName(kFirmwareAdrRule) : "FIXED_SF");
#else
        const auto roster = benchRoster();
        const uint32_t period = roster.periodUs ? roster.periodUs : multiclient::requiredPeriodUs(roster);
        std::snprintf(line, sizeof(line), "event=SCHEDULE_CONFIG,period_us=%" PRIu32
                      ",roster_count=%u,bench_payload_ceiling_bytes=%u,data_sf_policy=%s,adr_rule=%s", period, roster.count,
                      unsigned(multiclient::kBenchPayloadBytes), adaptDataSf ? "adaptive" : "fixed_sf7",
                      adaptDataSf ? adrRuleName(kFirmwareAdrRule) : "FIXED_SF");
#endif
        log_.text(line);
    }
#endif
#if defined(ADAPTIVE_LORA_OLED)
    if (display_) {
#if defined(ADAPTIVE_LORA_MULTICLIENT)
        if (multicli_) {
            const auto snapshot = multicli_->telemetry();
            const auto now = static_cast<uint64_t>(esp_timer_get_time());
            const auto freshness = snapshot.freshness(now);
            std::snprintf(line, sizeof(line), "event=V3_LINK_STATUS,freshness=%s,frame=%" PRIu32 ",beacon=%s",
                freshness == LinkFreshness::WAIT ? "WAIT" : freshness == LinkFreshness::ONLINE ? "ONLINE" : "STALE",
                snapshot.frame, loaded_.role == DeviceRole::CLIENT ?
                    (multicli_->searchingForBeacon(now) ? "search" : "synced") : "n/a");
            log_.text(line);
#if defined(ADAPTIVE_LORA_SECURE)
            std::snprintf(line, sizeof(line), "event=SECURITY_TIMING,auth_drops=%" PRIu32
                ",tx_us_max=%" PRIu32 ",rx_us_max=%" PRIu32 ",late_tx=%" PRIu32,
                snapshot.authDrops, snapshot.worstCryptoTxUs, snapshot.worstCryptoRxUs, snapshot.cryptoLate);
            log_.text(line);
#if !defined(ADAPTIVE_LORA_NATIVE_TEST)
            std::snprintf(line, sizeof(line), "event=SECURITY_MEMORY,heap_free_bytes=%lu"
                ",heap_min_bytes=%lu,loop_stack_min_free_bytes=%lu",
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
            log_.text(line);
#endif
#endif
            if (multiclient::kAdaptive) {
                std::snprintf(line, sizeof(line), "event=ADR_TIMING,guard_us=%" PRIu32
                    ",retune_lead_us=%" PRIu32 ",retunes=%" PRIu32 ",worst_phy_ready_us=%" PRIu32
                    ",acked_data_sf=%u", multiclient::kSlotGuardUs, multiclient::kRetuneLeadUs,
                    snapshot.retunes, snapshot.worstPhyReadyUs, unsigned(snapshot.confirmedDataSf));
                log_.text(line);
            }
        }
        std::snprintf(line, sizeof(line),
            "event=OLED_STATUS,result=%s,worst_i2c_us=%" PRIu32 ",worst_refresh_io_us=%" PRIu32
            ",worst_step_us=%" PRIu32 ",step_budget_us=%" PRIu32,
            display_->error() ? display_->error() : (display_->ready() ? "ready" : "starting"),
            display_->worstIoUs(), display_->worstRefreshUs(), worstDisplayStepUs_, kOledStepBudgetUs);
#else
        std::snprintf(line, sizeof(line),
            "event=OLED_STATUS,result=%s,worst_i2c_us=%" PRIu32 ",worst_refresh_io_us=%" PRIu32,
            display_->error() ? display_->error() : (display_->ready() ? "ready" : "starting"),
            display_->worstIoUs(), display_->worstRefreshUs());
#endif
        log_.text(line);
    }
#endif
}
void FirmwareRuntime::setup() {
    Serial.begin(kSerialBaud);
    loadError_ = store_.load(loaded_);
#if defined(ADAPTIVE_LORA_SECURE)
    if (!loadError_) loadError_ = security_.load(credentialStorage_, loaded_, entropy_);
#elif defined(ADAPTIVE_LORA_ROTATING)
    if (!loadError_ && loaded_.role == DeviceRole::CLIENT &&
        !multiclient::registeredClient(loaded_.nodeId, loaded_.gatewayId, multiclient::kRegisteredCount))
        loadError_ = "client_outside_registry";
#elif defined(ADAPTIVE_LORA_MULTICLIENT)
    if (!loadError_ && loaded_.role == DeviceRole::GATEWAY && !multiclient::kBenchRoster.valid(loaded_.gatewayId))
        loadError_ = "invalid_schedule_roster";
#elif defined(ADAPTIVE_LORA_SCHEDULED)
    if (!loadError_ && !ScheduledLink::supports(loaded_)) loadError_ = "unsupported_schedule_config";
#endif
    log_.begin(loaded_);
    console_.begin(loaded_);
#if defined(ADAPTIVE_LORA_OLED)
    if (display_) display_->start(static_cast<uint64_t>(esp_timer_get_time()));
#endif
    log_.event("BOOT", kFirmwareVersion);
    log_.event(loadError_ ? "CONFIG_INVALID" : "CONFIG_VALID", loadError_ ? loadError_ : "loaded");
    if (loadError_) {
        log_.event("PROVISIONING", "radio_disabled");
        status();
        return;
    }
    const int16_t code = radio_.begin();
    log_.event("RADIO_INIT", code ? "failed" : radio_.chip(), nullptr, nullptr, code);
    if (!code) {
#if defined(ADAPTIVE_LORA_MULTICLIENT)
#if defined(ADAPTIVE_LORA_SECURE)
        multicli_.emplace(loaded_, radio_, log_, payload_, freshBootId, security_.population(),
                          multiclient::kBenchPayloadBytes, security_, adaptDataSf);
#elif defined(ADAPTIVE_LORA_ROTATING)
        multicli_.emplace(loaded_, radio_, log_, payload_, freshBootId, multiclient::kRegisteredCount,
                          multiclient::kBenchPayloadBytes, adaptDataSf);
#else
        multicli_.emplace(loaded_, radio_, log_, payload_, freshBootId, benchRoster(), adaptDataSf);
#endif
#if defined(ADAPTIVE_LORA_OLED)
        if (loaded_.role == DeviceRole::CLIENT && display_ && !display_->error())
            clientDisplayUntil_ = static_cast<uint64_t>(esp_timer_get_time()) + kClientDisplayStartupUs;
#endif
#elif defined(ADAPTIVE_LORA_SCHEDULED)
        scheduled_.emplace(loaded_, radio_, log_, freshBootId);
#else
        if (loaded_.role == DeviceRole::GATEWAY) {
            gateway_.emplace(loaded_, radio_, log_, freshBootId);
            gateway_->begin(micros());
        } else {
            client_.emplace(loaded_, radio_, log_, freshBootId);
            client_->begin(micros());
        }
#endif
    }
    status();
}
void FirmwareRuntime::command(ConsoleAction action) {
    switch (action) {
        case ConsoleAction::HELP:
            log_.text("command=help|status|config show|config save|reboot");
            log_.text("command=config set role gateway|client");
            log_.text("command=config set network-id <1..65535>");
            log_.text("command=config set node-id <1..65534>");
            log_.text("command=config set gateway-id <1..65534>");
            log_.text("command=config set hello-interval-ms <1000..3600000>");
            break;
        case ConsoleAction::STATUS: status(); break;
        case ConsoleAction::SHOW: {
            const char* error = validateConfig(console_.staging());
            showConfig(console_.staging(), "staged", error ? error : "valid");
            break;
        }
        case ConsoleAction::SAVE: {
            const char* error = nullptr;
#if defined(ADAPTIVE_LORA_SECURE)
            if (radio_.initialized()) error = "secure_config_install_offline";
#endif
#if defined(ADAPTIVE_LORA_ROTATING)
            const auto& staging = console_.staging();
            if (!error && !validateConfig(staging) && staging.role == DeviceRole::CLIENT &&
                !multiclient::registeredClient(staging.nodeId, staging.gatewayId, multiclient::kRegisteredCount))
                error = "client_outside_registry";
#elif defined(ADAPTIVE_LORA_MULTICLIENT)
            if (console_.staging().role == DeviceRole::GATEWAY &&
                !multiclient::kBenchRoster.valid(console_.staging().gatewayId)) error = "invalid_schedule_roster";
#elif defined(ADAPTIVE_LORA_SCHEDULED)
            if (!validateConfig(console_.staging()) && !ScheduledLink::supports(console_.staging()))
                error = "unsupported_schedule_config";
#endif
            if (!error) error = store_.save(console_.staging());
            log_.event(error ? "CONFIG_ERROR" : "CONFIG_SAVED", error ? error : "verified_reboot_required");
            break;
        }
        case ConsoleAction::REBOOT:
            rebootPending_ = true;
            log_.event("REBOOT", "waiting_for_tx_completion");
            break;
        case ConsoleAction::UPDATED: log_.event("CONFIG_STAGED", "reboot_required_after_save"); break;
        case ConsoleAction::ERROR: log_.event("CONFIG_ERROR", console_.error()); break;
        case ConsoleAction::NONE: break;
    }
}
#if defined(ADAPTIVE_LORA_MULTICLIENT)
SubmitResult FirmwareRuntime::submitPayload(const uint8_t* bytes, size_t length) {
    if (!multicli_ || loaded_.role != DeviceRole::CLIENT || rebootPending_) return SubmitResult::INACTIVE;
#if defined(ADAPTIVE_LORA_ROTATING)
    if (length > multiclient::kBenchPayloadBytes) return SubmitResult::TOO_LARGE;
#endif
    return payload_.submit(bytes, length);
}

bool FirmwareRuntime::receivePayload(void* context, const PayloadDelivery& packet) {
    auto& log = static_cast<FirmwareRuntime*>(context)->log_;
    char line[256];
#if defined(ADAPTIVE_LORA_SECURE)
    const int written = std::snprintf(line, sizeof(line),
        "event=APP_PAYLOAD,source_id=%u,boot_id=%" PRIu32 ",frame_id=%" PRIu32 ",bytes=%u",
        packet.source, packet.clientBoot, packet.frame, packet.size);
    if (written < 0 || size_t(written) >= sizeof(line)) return false;
#else
    const int prefix = std::snprintf(line, sizeof(line),
        "event=APP_PAYLOAD,source_id=%u,boot_id=%" PRIu32 ",frame_id=%" PRIu32 ",bytes=%u,hex=",
        packet.source, packet.clientBoot, packet.frame, packet.size);
    if (prefix < 0 || size_t(prefix) + size_t(packet.size) * 2 >= sizeof(line)) return false;
    constexpr char digits[] = "0123456789abcdef";
    size_t end = static_cast<size_t>(prefix);
    for (uint8_t i = 0; i < packet.size; ++i) {
        line[end++] = digits[packet.bytes[i] >> 4];
        line[end++] = digits[packet.bytes[i] & 15];
    }
    line[end] = '\0';
#endif
    const auto drops = log.dropped();
    log.text(line);
    return log.dropped() == drops;
}
#endif
void FirmwareRuntime::loop() {
    const auto event = radio_.poll(static_cast<uint64_t>(esp_timer_get_time()));
    // Radio calls may wait; apps must classify the completed event at fresh time.
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    const auto appNow = static_cast<uint64_t>(esp_timer_get_time());
#if defined(ADAPTIVE_LORA_OLED)
    if (clientDisplayUntil_) {
        if (display_->ready() || display_->error()) clientDisplayUntil_ = 0;
        else if (appNow >= clientDisplayUntil_) {
            clientDisplayUntil_ = 0;
            log_.event("OLED_BOOT", "continue_while_searching");
        }
    }
    const bool waitingForBootScreen = clientDisplayUntil_ != 0;
#else
    constexpr bool waitingForBootScreen = false;
#endif
    if (multicli_ && !waitingForBootScreen) {
        if (example_ && loaded_.role == DeviceRole::CLIENT && appNow >= nextReport_) {
            nextReport_ = appNow + uint64_t(loaded_.helloIntervalMs) * 1000;
            if (multiclient::kAdaptive && !payload_.pending()) {
                char sample[sizeof("ID=0000|SEQ=0000|MSG=HELLO WORLD")];
                static_assert(!multiclient::kAdaptive || sizeof(sample) - 1 == multiclient::kBenchPayloadBytes,
                              "Synthetic sample must match the adaptive bench ceiling");
                const int length = std::snprintf(sample, sizeof(sample), "ID=%04X|SEQ=%04X|MSG=HELLO WORLD",
                    unsigned(loaded_.nodeId), unsigned(exampleSequence_));
                if (length == static_cast<int>(sizeof(sample) - 1) &&
                    submitPayload(reinterpret_cast<const uint8_t*>(sample), sizeof(sample) - 1) == SubmitResult::QUEUED)
                    ++exampleSequence_;
            } else if (!multiclient::kAdaptive) {
                constexpr uint8_t example[] = {0x01, 0x00, 0x16, 0x80, 0x00, 0x05, 0x00, 0x7f};
                submitPayload(example, sizeof(example));
            }
        }
        multicli_->update(appNow, event, !rebootPending_);
    }
#elif defined(ADAPTIVE_LORA_SCHEDULED)
    if (scheduled_) scheduled_->update(static_cast<uint64_t>(esp_timer_get_time()), event, !rebootPending_);
#else
    const uint32_t appNow = micros();
    if (gateway_) gateway_->update(appNow, event, !rebootPending_);
    if (client_) client_->update(appNow, event, !rebootPending_);
#endif
    for (unsigned budget = 0; budget < 64 && Serial.available(); ++budget)
        command(console_.feed(static_cast<char>(Serial.read())));
    if (rebootPending_ && !rebootStopped_ && !radio_.transmitting()) {
        const int16_t code = radio_.stop();
        log_.event(code ? "RADIO_ERROR" : "REBOOT", code ? "standby_failed_restarting" : "radio_stopped", nullptr, nullptr, code);
        rebootStopped_ = true;
        rebootAt_ = micros() + 100000;
    }
    log_.drain();
    if (rebootStopped_ && deadlineReached(micros(), rebootAt_)) {
        ESP.restart();
        return;
    }
#if defined(ADAPTIVE_LORA_OLED)
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    const auto displayNow = static_cast<uint64_t>(esp_timer_get_time());
    if (safeToRefreshDisplay(displayNow)) serviceDisplay(displayNow);
#else
    if (display_ && !rebootPending_ && !radio_.transmitting() && !(client_ && client_->awaitingAck()))
        serviceDisplay(static_cast<uint64_t>(esp_timer_get_time()));
#endif
#endif
}
#if defined(ADAPTIVE_LORA_OLED)
#if defined(ADAPTIVE_LORA_MULTICLIENT)
bool FirmwareRuntime::safeToRefreshDisplay(uint64_t now) const {
    if (!display_ || !display_->canService(now) || rebootPending_) return false;
    if (clientDisplayUntil_) {
        // The client role has yet to start; the initialized radio is in standby.
        return now < clientDisplayUntil_ && clientDisplayUntil_ - now >= kOledStepBudgetUs &&
               !radio_.receiving() && !radio_.transmitting();
    }
    return !radio_.initialized() ||
        (multicli_ && multicli_->safeToRefreshDisplay(now, kOledStepBudgetUs) && !radio_.irqPending());
}
#endif
void FirmwareRuntime::serviceDisplay(uint64_t now) {
    if (display_->needsSnapshot(now)) {
        DisplaySnapshot snapshot;
        snapshot.config = loaded_;
        snapshot.configError = loadError_;
        snapshot.radioReady = radio_.healthy();
        snapshot.radioState = radio_.stateName();
        snapshot.logDropped = log_.dropped();
#if defined(ADAPTIVE_LORA_MULTICLIENT)
        snapshot.protocol = wireVersion;
        if (multicli_) {
            snapshot.multiclient = multicli_->telemetry();
            snapshot.searchingForBeacon = multicli_->searchingForBeacon(now);
        }
        else if (!loadError_) snapshot.multiclient.radioErrors = 1;
#else
        if (gateway_) snapshot.gateway = gateway_->telemetry();
        if (client_) snapshot.client = client_->telemetry();
#endif
        display_->render(snapshot, now);
    }
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    // Formatting may use part of the window. Check again before any Wire call.
    const auto serviceAt = static_cast<uint64_t>(esp_timer_get_time());
    if (safeToRefreshDisplay(serviceAt)) display_->service(serviceAt);
    const auto elapsed = static_cast<uint32_t>(esp_timer_get_time() - now);
    if (elapsed > worstDisplayStepUs_) worstDisplayStepUs_ = elapsed;
    if (elapsed > kOledStepBudgetUs) display_->stopOnOverrun();
#else
    display_->service(now);
#endif
    if (display_->error() && !displayErrorLogged_) {
        displayErrorLogged_ = true;
        log_.event(displayReadyLogged_ ? "OLED_ERROR" : "OLED_INIT", display_->error());
    } else if (display_->ready() && (!displayReadyLogged_ || displayErrorLogged_)) {
        displayReadyLogged_ = true;
        displayErrorLogged_ = false;
        log_.event("OLED_INIT", "ready");
    }
}
#endif
}  // namespace adaptive_lora
