#pragma once
#include "adr_policy.h"
#include "payload_port.h"
#include "common/diagnostics/serial_logger.h"
#include "common/diagnostics/display_snapshot.h"
#if defined(ADAPTIVE_LORA_SECURE)
#include "secure_link.h"
#endif

namespace adaptive_lora {
class MulticlientLink {
public:
    using BootIdSource = uint32_t (*)(uint32_t);
    MulticlientLink(const DeviceConfig& config, RadioTransport& radio, SerialLogger& log,
#if defined(ADAPTIVE_LORA_ROTATING)
                    PayloadPort& payload, BootIdSource bootId, uint16_t registered, uint8_t payloadCeiling,
#if defined(ADAPTIVE_LORA_SECURE)
                    SecureLink& security,
#endif
                    bool adaptDataSf = multiclient::kAdaptive, AdrRule adrRule = kFirmwareAdrRule);
#else
                    PayloadPort& payload, BootIdSource bootId, multiclient::Roster roster = {},
                    bool adaptDataSf = multiclient::kAdaptive, AdrRule adrRule = kFirmwareAdrRule);
#endif
    void update(uint64_t now, const RadioEvent& event, bool allowTx = true);
    MulticlientTelemetry telemetry() const;
    bool safeToRefreshDisplay(uint64_t now, uint32_t stepBudgetUs) const;
    bool searchingForBeacon(uint64_t now) const;

private:
    enum class Phase { WAIT, BEACON_TX, REQUEST, GRANT_TX, DATA };
    enum class Reception : uint8_t { NONE, ACCEPTED, REFUSED };
    struct Received {
        uint64_t completedAt = 0;
        Reception result = Reception::NONE;
        uint8_t bytes[multiclient::kMaxData]{};
        float rssi = 0;
        float snr = 0;
        float requestSnr = kAdrMissingSnr;
        bool metrics = false;
        bool retried = false;
        bool rxWindowUnobserved = false;
        bool primaryMiss = false;
        bool recoveryConfirmed = false;
    };
    Frame frame(MessageType type, uint16_t sequence) const;
    bool send(const Frame& frame, uint64_t now, const char* event, uint64_t slotStart);
    void gateway(uint64_t now, const RadioEvent& event, const Frame* incoming);
    void gatewayRequest(uint64_t now, uint64_t receivedAt, const Frame* incoming, const RadioEvent& event);
    void gatewayData(uint64_t now, const Frame* incoming, const RadioEvent& event);
    void client(uint64_t now, const Frame* incoming, const RadioEvent& event);
    void acceptBeacon(uint64_t now, const Frame& beacon);
    void resetClientPeer();
    void acceptFeedback(uint64_t now, const Frame& ack, const RadioEvent& event);
    void clientFinish(PayloadResult result);
    void observePeer(uint64_t now, uint16_t peer, const RadioEvent& event);
    void logLinkSamples(bool timingOk);
    void invalidatePrimaryRx(uint64_t at);
    bool prepareSf(uint8_t sf, uint64_t now);
    void admitProfiles();
    size_t adrIndex(uint8_t slot) const;
    uint32_t opportunity(uint8_t slot) const;
#if defined(ADAPTIVE_LORA_ROTATING)
    bool selectGroup();
#endif
#if defined(ADAPTIVE_LORA_SECURE)
    bool expectedSecurePacket(const RadioEvent& event, uint64_t receivedAt) const;
    SecureLink& security_;
    uint8_t secureAuxiliary_[secure::challengeBytes]{};
#endif
    void logAdrFields(char* buffer, size_t size, uint8_t index, const AdrObservation& obs, const AdrReview& review) const;

    DeviceConfig config_;
    RadioTransport& radio_;
    SerialLogger& log_;
    PayloadPort& payload_;
    BootIdSource bootIdSource_;
    const bool adaptDataSf_;
    const AdrRule adrRule_;
    multiclient::Roster roster_;
    multiclient::Plan plan_;
    Received received_[multiclient::kMaxClients]{};
#if defined(ADAPTIVE_LORA_ROTATING)
    multiclient::Rotation rotation_;
    const uint8_t payloadCeiling_;
    AdrState adr_[multiclient::kPopulationLimit]{};
    uint32_t opportunities_[multiclient::kPopulationLimit]{};
#else
    AdrState adr_[multiclient::kAdaptive ? multiclient::kMaxClients : 1]{};
#endif
    Frame data_{};
    Phase phase_ = Phase::WAIT;
    uint8_t round_ = 0;
    uint8_t mask_ = 0;
    uint8_t rxReadyMask_ = 0;  // Gateway saw RX armed at the slot SF before launch, or received DATA.
    bool launched_ = false;
    bool sentData_ = false;
    bool validConfig_ = false;
    bool frameTimingOk_ = false;
    uint8_t index_ = 0;
    uint32_t boot_ = 0;
    uint32_t gatewayBoot_ = 0;
    uint32_t retiredBoot_ = 0;
    uint32_t frameId_ = 0;
    uint32_t acknowledgedFrame_ = 0;
    uint64_t beaconAt_ = 0;
    uint64_t dataAt_ = 0;
    uint64_t controlStart_ = 0;
    uint64_t nextBeacon_ = 0;
    uint64_t nextRecovery_ = 0;
    uint64_t nextBoot_ = 0;
    uint64_t nextRetune_ = 0;
    uint64_t phyRequestedAt_ = 0;
    bool phyReadinessPending_ = false;
    uint8_t radioSf_ = multiclient::kBaseSf;
    uint64_t displayQuietAt_ = 0;
    uint64_t displayQuietUntil_ = 0;
    MulticlientTelemetry telemetry_;
    BitmapTelemetry pendingBitmap_;
};
}
