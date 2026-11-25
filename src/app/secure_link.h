// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#if defined(ADAPTIVE_LORA_SECURE)
#include "config_storage.h"
#include "common/config/device_config.h"
#include "common/protocol/multiclient_protocol.h"
#include "common/protocol/secure_crypto.h"

namespace adaptive_lora {
// The unencrypted schedule codec is internal to v7. Only this envelope is
// allowed to cross the radio boundary with the secure build.
class SecureLink {
public:
    const char* load(ConfigStorage& storage, const DeviceConfig& device, secure::Entropy entropy);
    uint16_t population() const { return population_; }
    uint32_t newBoot(uint32_t previous);
    bool sameEpoch(const uint8_t* epoch) const;
    bool retiredEpoch(const uint8_t* epoch) const;
    bool confirmedEpoch(const uint8_t* epoch) const;
    bool candidateConfirmed() const { return confirmedEpoch(epoch_); }
    uint32_t confirmedFrame() const { return confirmedFrame_; }
    void acceptBeacon(const uint8_t* epoch, uint32_t frame);
    bool confirmGrant(uint32_t frame);
    void beginGatewayFrame();
    void acceptRequest(uint8_t slot, const uint8_t* challenge);
    bool seal(const Frame& frame, const multiclient::Roster& roster, uint8_t localSlot,
              uint8_t* bytes, size_t& length);
    bool open(const uint8_t* bytes, size_t length, const multiclient::Roster& roster, uint8_t localSlot,
              Frame& frame, uint8_t* auxiliary);

private:
    const uint8_t* peerKey(uint16_t id) const;
    bool authenticateRequest(const uint8_t* key, const uint8_t* bytes, size_t length, uint8_t* tag);
    bool protectData(bool encrypt, uint16_t client, const uint8_t* proof, const uint8_t* header,
                     const uint8_t* input, uint8_t* output, size_t length, uint8_t* tag);
    bool signature(bool sign, const uint8_t* epoch, const uint8_t* bytes, size_t signedBytes, uint8_t* tag);
    bool isGateway() const { return config_.role == DeviceRole::GATEWAY; }

    static constexpr size_t recordPrefix = 12 + secure::epochBytes;
    DeviceConfig config_;
    uint16_t population_ = 0;
    secure::Crypto crypto_;
    uint8_t record_[recordPrefix + secure::privateKeyBytes + multiclient::kPopulationLimit * secure::keyBytes]{};
    uint8_t epoch_[secure::epochBytes]{};
    uint8_t retired_[secure::epochBytes]{};
    uint8_t confirmed_[secure::epochBytes]{};
    uint32_t confirmedFrame_ = 0;
    uint8_t proof_[secure::challengeBytes]{};
    uint8_t requested_[multiclient::kMaxClients][secure::challengeBytes]{};
    uint8_t cachedData_[kMaxFrameSize + secure::tagBytes]{};
    size_t cachedLength_ = 0;
};
}
#endif
