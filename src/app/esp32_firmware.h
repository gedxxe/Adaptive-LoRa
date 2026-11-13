#pragma once
#include "firmware_runtime.h"
#include "preferences_config_storage.h"
#include "common/radio/llcc68_hardware.h"

namespace adaptive_lora {
// Hardware owners precede and outlive the sole production coordinator. Native
// tests construct that same coordinator with low-level fakes instead.
class Esp32Firmware {
public:
    void setup() { runtime_.setup(); }
    void loop() { runtime_.loop(); }
private:
    PreferencesConfigStorage configStorage_;
    Llcc68Hardware radioHardware_;
    FirmwareRuntime runtime_{configStorage_, radioHardware_};
};
}  // namespace adaptive_lora
