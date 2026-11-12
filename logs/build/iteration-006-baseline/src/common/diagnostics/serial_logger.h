#pragma once
#include "common/config/device_config.h"
#include "common/protocol/frame_codec.h"
#include "common/radio/radio_driver.h"

namespace adaptive_lora {
class SerialLogger {
public:
    void begin(const DeviceConfig& config) { config_ = config; }
    void event(const char* name, const char* result, const Frame* frame = nullptr,
               const RadioEvent* rx = nullptr, int16_t code = 0, uint32_t toa = 0);
    void text(const char* line);
    void drain();
    bool empty() const { return count_ == 0; }
    uint32_t dropped() const { return dropped_; }
private:
    DeviceConfig config_{};
    char lines_[16][512]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    size_t offset_ = 0;
    uint32_t dropped_ = 0;
};
}  // namespace adaptive_lora
