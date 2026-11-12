#include <Arduino.h>

#include "app/esp32_firmware.h"

namespace {
adaptive_lora::Esp32Firmware runtime;
}

void setup() {
    runtime.setup();
}

void loop() {
    runtime.loop();
}
