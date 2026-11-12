#include <Arduino.h>

#include "app/firmware_runtime.h"

namespace {
adaptive_lora::FirmwareRuntime runtime;
}

void setup() {
    runtime.setup();
}

void loop() {
    runtime.loop();
}
