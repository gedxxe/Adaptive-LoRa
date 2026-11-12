#include "firmware_runtime.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_random.h>
#include <cstdio>
#include <inttypes.h>

namespace adaptive_lora {
namespace {
uint32_t freshBootId(uint32_t previous) {
    // ESP32 hardware entropy source; RF/Wi-Fi is not enabled by this application.
    uint32_t value;
    do { value = esp_random(); } while (value == 0 || value == previous);
    return value;
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
        "event=STATUS,mode=%s,radio=%s,chip=%s,fw=%s,protocol=%u,build_id=%s,reset_reason=%d,log_dropped=%" PRIu32,
        loadError_ ? "PROVISIONING" : (radio_.healthy() ? roleName(loaded_.role) : "RADIO_FAULT"),
        radio_.initialized() ? (radio_.healthy() ? "ready" : "fault") : "disabled",
        radio_.initialized() ? radio_.chip() : "N/A", kFirmwareVersion, kProtocolVersion, kBuildId,
        static_cast<int>(esp_reset_reason()), log_.dropped());
    log_.text(line);
}
void FirmwareRuntime::setup() {
    Serial.begin(kSerialBaud);
    loadError_ = store_.load(loaded_);
    log_.begin(loaded_);
    console_.begin(loaded_);
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
        if (loaded_.role == DeviceRole::GATEWAY) {
            gateway_.emplace(loaded_, radio_, log_, freshBootId);
            gateway_->begin(micros());
        } else {
            client_.emplace(loaded_, radio_, log_, freshBootId);
            client_->begin(micros());
        }
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
            const char* error = store_.save(console_.staging());
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
    if (gateway_) gateway_->update(now, event, !rebootPending_);
    if (client_) client_->update(now, event, !rebootPending_);
    for (unsigned budget = 0; budget < 64 && Serial.available(); ++budget)
        command(console_.feed(static_cast<char>(Serial.read())));
    if (rebootPending_ && !rebootStopped_ && !radio_.transmitting()) {
        const int16_t code = radio_.stop();
        log_.event(code ? "RADIO_ERROR" : "REBOOT", code ? "standby_failed_restarting" : "radio_stopped", nullptr, nullptr, code);
        rebootStopped_ = true;
        rebootAt_ = now + 100000;
    }
    log_.drain();
    if (rebootStopped_ && deadlineReached(now, rebootAt_)) ESP.restart();
}
}  // namespace adaptive_lora
