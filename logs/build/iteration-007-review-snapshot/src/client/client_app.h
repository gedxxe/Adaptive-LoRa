#pragma once
#include "common/diagnostics/serial_logger.h"
#include "common/protocol/sequence_tracker.h"

namespace adaptive_lora {
class ClientApp {
public:
    ClientApp(const DeviceConfig& config, RadioTransport& radio, SerialLogger& log, BootIdGenerator generate);
    void begin(uint32_t now);
    void update(uint32_t now, const RadioEvent& event, bool allowNew = true);
private:
    enum class State { IDLE, TRANSMITTING, WAIT_ACK };
    void finish(const char* result, const RadioEvent* event = nullptr, bool txFailure = false);
    DeviceConfig config_;
    RadioTransport& radio_;
    SerialLogger& log_;
    BootIdGenerator generate_;
    SequenceCounter sequence_;
    Frame outstanding_{};
    State state_ = State::IDLE;
    uint64_t nextHelloUs_ = 0;
    uint32_t ackDeadline_ = 0;
    uint32_t recoverAt_ = 0;
    unsigned recoveryAttempts_ = 0;
    uint32_t attempts_ = 0;
    uint32_t successes_ = 0;
    uint32_t timeouts_ = 0;
    uint32_t txFailures_ = 0;
    uint64_t sentAtUs_ = 0;
};
}
