#pragma once
#include "common/diagnostics/serial_logger.h"
#include "common/protocol/sequence_tracker.h"

namespace adaptive_lora {
class GatewayApp {
public:
    GatewayApp(const DeviceConfig& config, RadioDriver& radio, SerialLogger& log, BootIdGenerator generate);
    void begin(uint32_t now);
    void update(uint32_t now, const RadioEvent& event, bool allowNew = true);
private:
    void listen(uint32_t now);
    DeviceConfig config_;
    RadioDriver& radio_;
    SerialLogger& log_;
    BootIdGenerator generate_;
    SequenceCounter sequence_;
    SequenceTracker tracker_;
    Frame ack_{};
    uint32_t recoverAt_ = 0;
    unsigned recoveryAttempts_ = 0;
};
}
