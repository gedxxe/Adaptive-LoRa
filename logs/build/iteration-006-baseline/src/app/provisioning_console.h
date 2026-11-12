#pragma once
#include <cstddef>
#include "common/config/device_config.h"

namespace adaptive_lora {
enum class ConsoleAction { NONE, HELP, STATUS, SHOW, SAVE, REBOOT, UPDATED, ERROR };
class ProvisioningConsole {
public:
    void begin(const DeviceConfig& staging) { staging_ = staging; }
    ConsoleAction feed(char byte);
    const DeviceConfig& staging() const { return staging_; }
    const char* error() const { return error_; }
private:
    ConsoleAction execute();
    char line_[128]{};
    size_t length_ = 0;
    bool overflow_ = false;
    DeviceConfig staging_{};
    const char* error_ = "none";
};
}  // namespace adaptive_lora
