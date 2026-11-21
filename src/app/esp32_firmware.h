#pragma once
#include "firmware_runtime.h"
#include "preferences_config_storage.h"
#include "common/radio/sx1262_hardware.h"

namespace adaptive_lora {
// Hardware owners precede and outlive the sole production coordinator. Native
// tests construct that same coordinator with low-level fakes instead.
class Esp32Firmware {
public:
    void setup() { runtime_.setup(); }
    void loop() { runtime_.loop(); }
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    SubmitResult submitPayload(const uint8_t* bytes, size_t length) { return runtime_.submitPayload(bytes, length); }
    PayloadResult payloadResult() const { return runtime_.payloadResult(); }
#endif
private:
    PreferencesConfigStorage configStorage_;
    Sx1262Hardware radioHardware_;
#if defined(ADAPTIVE_LORA_OLED)
    OledStatusDisplay display_;
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    FirmwareRuntime runtime_{configStorage_, radioHardware_, true, nullptr, nullptr, &display_};
#else
    FirmwareRuntime runtime_{configStorage_, radioHardware_, &display_};
#endif
#else
    FirmwareRuntime runtime_{configStorage_, radioHardware_};
#endif
};
}  // namespace adaptive_lora
