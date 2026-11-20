#include "firmware_runtime.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <cstdio>
#include <cinttypes>

namespace adaptive_lora {
namespace {
#if defined(ADAPTIVE_LORA_SCHEDULED)
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
#if defined(ADAPTIVE_LORA_OLED)
    if (display_) {
        std::snprintf(line, sizeof(line),
            "event=OLED_STATUS,result=%s,worst_i2c_us=%" PRIu32 ",worst_refresh_io_us=%" PRIu32,
            display_->error() ? display_->error() : (display_->ready() ? "ready" : "starting"),
            display_->worstIoUs(), display_->worstRefreshUs());
        log_.text(line);
    }
#endif
}
void FirmwareRuntime::setup() {
    Serial.begin(kSerialBaud);
    loadError_ = store_.load(loaded_);
#if defined(ADAPTIVE_LORA_SCHEDULED)
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
#if defined(ADAPTIVE_LORA_SCHEDULED)
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
#if defined(ADAPTIVE_LORA_SCHEDULED)
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
void FirmwareRuntime::loop() {
    const uint32_t now = micros();
    const auto event = radio_.poll(now);
    // Radio calls may wait; apps must classify the completed event at fresh time.
#if defined(ADAPTIVE_LORA_SCHEDULED)
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
    if (display_ && !rebootPending_ && !radio_.transmitting() && !(client_ && client_->awaitingAck()))
        serviceDisplay();
#endif
}
#if defined(ADAPTIVE_LORA_OLED)
void FirmwareRuntime::serviceDisplay() {
    const auto now = static_cast<uint64_t>(esp_timer_get_time());
    if (display_->needsSnapshot(now)) {
        DisplaySnapshot snapshot;
        snapshot.config = loaded_;
        snapshot.configError = loadError_;
        snapshot.radioReady = radio_.healthy();
        snapshot.radioState = radio_.stateName();
        snapshot.logDropped = log_.dropped();
        if (gateway_) snapshot.gateway = gateway_->telemetry();
        if (client_) snapshot.client = client_->telemetry();
        display_->render(snapshot, now);
    }
    display_->service(now);
    if (display_->error() && !displayErrorLogged_) {
        displayErrorLogged_ = true;
        log_.event(displayReadyLogged_ ? "OLED_ERROR" : "OLED_INIT", display_->error());
    } else if (display_->ready() && !displayReadyLogged_) {
        displayReadyLogged_ = true;
        log_.event("OLED_INIT", "ready");
    }
}
#endif
}  // namespace adaptive_lora
