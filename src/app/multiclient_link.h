#pragma once
#include "payload_port.h"
#include "common/diagnostics/serial_logger.h"
#include "common/diagnostics/display_snapshot.h"

namespace adaptive_lora {
class MulticlientLink {
public:
    using BootIdSource = uint32_t (*)(uint32_t);
    MulticlientLink(const DeviceConfig& config, RadioTransport& radio, SerialLogger& log,
                    PayloadPort& payload, BootIdSource bootId, multiclient::Roster roster = {});
    void update(uint64_t now, const RadioEvent& event, bool allowTx = true);
    MulticlientTelemetry telemetry() const;
    bool safeToRefreshDisplay(uint64_t now, uint32_t stepBudgetUs) const;
    bool searchingForBeacon(uint64_t now) const;

private:
    enum class Phase { WAIT, BEACON_TX, REQUEST, GRANT_TX, DATA };
    enum class Reception : uint8_t { NONE, ACCEPTED, REFUSED };
    struct Received {
        Reception result = Reception::NONE;
        uint8_t bytes[multiclient::kMaxData]{};
        float rssi = 0;
        float snr = 0;
        bool metrics = false;
        bool retried = false;
    };

    Frame frame(MessageType type, uint16_t sequence) const;
    bool send(const Frame& frame, uint64_t now, const char* event);
    void gateway(uint64_t now, const RadioEvent& event, const Frame* incoming);
    void gatewayRequest(uint64_t now, uint64_t receivedAt, const Frame* incoming);
    void gatewayData(uint64_t now, const Frame* incoming, const RadioEvent& event);
    void client(uint64_t now, const Frame* incoming, const RadioEvent& event);
    void acceptBeacon(uint64_t now, const Frame& beacon);
    void acceptFeedback(uint64_t now, const Frame& ack, const RadioEvent& event);
    void clientFinish(PayloadResult result);
    void observePeer(uint64_t now, uint16_t peer, const RadioEvent& event);
    void logLinkSamples();

    DeviceConfig config_;
    RadioTransport& radio_;
    SerialLogger& log_;
    PayloadPort& payload_;
    BootIdSource bootIdSource_;
    multiclient::Roster roster_;
    multiclient::Plan plan_;
    Received received_[multiclient::kMaxClients]{};
    Frame data_{};
    Phase phase_ = Phase::WAIT;
    uint8_t round_ = 0;
    uint8_t mask_ = 0;
    bool launched_ = false;
    bool sentData_ = false;
    bool validConfig_ = false;
    bool frameTimingOk_ = false;
    uint8_t index_ = 0;
    uint32_t boot_ = 0;
    uint32_t gatewayBoot_ = 0;
    uint32_t retiredBoot_ = 0;
    uint32_t frameId_ = 0;
    uint64_t beaconAt_ = 0;
    uint64_t dataAt_ = 0;
    uint64_t controlStart_ = 0;
    uint64_t nextBeacon_ = 0;
    uint64_t nextRecovery_ = 0;
    uint64_t nextBoot_ = 0;
    uint64_t displayQuietAt_ = 0;
    uint64_t displayQuietUntil_ = 0;
    MulticlientTelemetry telemetry_;
    BitmapTelemetry pendingBitmap_;
};
}
