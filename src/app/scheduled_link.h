#pragma once
#include "common/diagnostics/serial_logger.h"
#include "common/protocol/scheduled_protocol.h"

namespace adaptive_lora {
// Both roles use the same slot calendar. The driver remains the radio owner.
class ScheduledLink {
public:
    using BootIdSource = uint32_t (*)(uint32_t);
    ScheduledLink(const DeviceConfig& config, RadioTransport& radio, SerialLogger& log,
                  BootIdSource bootId, uint8_t payloadBudget = scheduled::kDemoSize);
    static bool supports(const DeviceConfig& config);
    void update(uint64_t now, const RadioEvent& event, bool allowTx);

private:
    Frame frame(MessageType type, uint16_t sequence) const;
    bool send(const Frame& outgoing, uint64_t now, const char* name);
    void gateway(uint64_t now, const RadioEvent& event, const Frame* incoming);
    void client(uint64_t now, const RadioEvent& event, const Frame* incoming);
    void acceptBeacon(uint64_t now, const Frame& beacon, const RadioEvent& event);

    DeviceConfig config_;
    RadioTransport& radio_;
    SerialLogger& log_;
    BootIdSource bootIdSource_;
    scheduled::Schedule schedule_;
    uint32_t bootId_ = 0;
    uint32_t gatewayBoot_ = 0;
    uint32_t retiredGateway_ = 0;
    uint32_t frameId_ = 0;
    uint64_t anchor_ = 0;
    uint64_t nextBeacon_ = 0;
    uint64_t beaconDeadline_ = 0;
    uint64_t nextRxAttempt_ = 0;
    uint64_t nextBootAttempt_ = 0;
    Frame data_{};
    uint8_t round_ = 0;
    bool beaconPending_ = false;
    bool active_ = false;
    bool haveData_ = false;
    bool attempted_ = false;
    bool dataSent_ = false;
};
}
