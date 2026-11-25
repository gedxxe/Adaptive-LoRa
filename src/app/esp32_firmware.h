#pragma once
#include "firmware_runtime.h"
#include "preferences_config_storage.h"
#include "common/radio/sx1262_hardware.h"
#if defined(ADAPTIVE_LORA_SECURE)
#include <bootloader_random.h>
#include <esp_random.h>
#endif

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
#if defined(ADAPTIVE_LORA_SECURE)
    static int startupEntropy(void*, unsigned char* bytes, size_t size) {
        bootloader_random_enable();
        esp_fill_random(bytes, size);
        bootloader_random_disable();
        // Reject a stuck output. This is a startup sanity check, not an
        // entropy estimate or a replacement for the SDK entropy source.
        for (size_t i = 1; i < size; ++i)
            if (bytes[i] != bytes[0]) return 0;
        return -1;
    }
#if defined(ADAPTIVE_LORA_SECURE_DEMO)
    static constexpr bool demo = true;
#else
    static constexpr bool demo = false;
#endif
    FirmwareRuntime runtime_{configStorage_, radioHardware_, demo, nullptr, nullptr, &display_, startupEntropy};
#else
    FirmwareRuntime runtime_{configStorage_, radioHardware_, true, nullptr, nullptr, &display_};
#endif
#else
    FirmwareRuntime runtime_{configStorage_, radioHardware_, &display_};
#endif
#else
    FirmwareRuntime runtime_{configStorage_, radioHardware_};
#endif
};
}  // namespace adaptive_lora
