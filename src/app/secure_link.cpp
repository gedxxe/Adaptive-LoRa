// SPDX-License-Identifier: GPL-3.0-only
#include "secure_link.h"
#if defined(ADAPTIVE_LORA_SECURE)
#include <cstring>
#include <mbedtls/platform_util.h>
#include "research_roster.h"

namespace adaptive_lora {
using namespace secure;
using namespace multiclient;
namespace {
constexpr char credentialNamespace[] = "alora_sec";
constexpr char credentialRecord[] = "v7";
constexpr uint8_t format[] = {'A', 'L', '0', '7'};
constexpr uint8_t requestLabel[] = "Adaptive-LoRa/v7/request";
constexpr uint8_t dataLabel[] = "Adaptive-LoRa/v7/data";
constexpr uint8_t zero[challengeBytes]{};
static_assert(epochBytes == challengeBytes);
static_assert(kHeaderSize + grantBytes(kMaxClients) + kMaxClients * echoBytes + signatureBytes == maxPacket);
static_assert(maxPacket <= 255);

size_t overhead(MessageType type, uint8_t slots) {
    switch (type) {
        case MessageType::BEACON: return epochBytes + signatureBytes;
        case MessageType::SLOT_GRANT: return slots * echoBytes + signatureBytes;
        case MessageType::ACK_BITMAP: return signatureBytes;
        case MessageType::PAYLOAD_REQUEST: return challengeBytes + tagBytes;
        case MessageType::DATA: return tagBytes;
        default: return 0;
    }
}

bool gatewayMessage(MessageType type) {
    return type == MessageType::BEACON || type == MessageType::SLOT_GRANT || type == MessageType::ACK_BITMAP;
}
}

const char* SecureLink::load(ConfigStorage& storage, const DeviceConfig& device, Entropy entropy) {
    population_ = 0;
    config_ = device;
    if (validateConfig(device) || !storage.begin(credentialNamespace, true)) return "security_missing";
    const size_t length = storage.getBytesLength(credentialRecord);
    const bool read = length >= recordPrefix && length <= sizeof(record_) &&
        storage.getBytes(credentialRecord, record_, length) == length;
    storage.end();
    if (!read || std::memcmp(record_, format, sizeof(format)) ||
        record_[4] != uint8_t(device.role) || get16(record_ + 5) != device.networkId ||
        get16(record_ + 7) != device.gatewayId || get16(record_ + 9) != device.nodeId ||
        equal(record_ + 12, zero, epochBytes)) return "security_invalid_record";
    const uint16_t count = record_[11];
    if (!count || count > kRegisteredCount || (!isGateway() && !registeredClient(device.nodeId, device.gatewayId, count)))
        return "security_invalid_registry";
    const size_t expected = recordPrefix + (isGateway() ? privateKeyBytes + count * keyBytes : publicKeyBytes + keyBytes);
    if (length != expected) return "security_invalid_length";
    const uint8_t* keys = record_ + recordPrefix + (isGateway() ? privateKeyBytes : publicKeyBytes);
    const uint16_t slots = isGateway() ? count : 1;
    bool present = false;
    for (uint16_t i = 0; i < slots; ++i) {
        if (equal(keys + i * keyBytes, zero, keyBytes)) continue; // Revoked Gateway entry.
        present = true;
        for (uint16_t j = 0; j < i; ++j)
            if (equal(keys + i * keyBytes, keys + j * keyBytes, keyBytes)) return "security_duplicate_key";
    }
    if (!present || !crypto_.begin(isGateway(), record_ + recordPrefix, entropy)) return "security_unavailable";
    if (isGateway()) mbedtls_platform_zeroize(record_ + recordPrefix, privateKeyBytes);
    population_ = count;
    return nullptr;
}

const uint8_t* SecureLink::peerKey(uint16_t id) const {
    if (!registeredClient(id, config_.gatewayId, population_)) return nullptr;
    if (!isGateway() && id != config_.nodeId) return nullptr;
    const auto* key = isGateway() ?
        record_ + recordPrefix + privateKeyBytes + registryPosition(id, config_.gatewayId) * keyBytes :
        record_ + recordPrefix + publicKeyBytes;
    return equal(key, zero, keyBytes) ? nullptr : key;
}

uint32_t SecureLink::newBoot(uint32_t previous) {
    uint8_t fresh[epochBytes];
    if (!population_ || !crypto_.random(fresh, sizeof(fresh))) return 0;
    const auto id = get32(fresh);
    if (!id || id == previous) return 0;
    if (isGateway()) std::memcpy(epoch_, fresh, epochBytes);
    return id;
}

bool SecureLink::sameEpoch(const uint8_t* epoch) const { return equal(epoch_, epoch, epochBytes); }
bool SecureLink::retiredEpoch(const uint8_t* epoch) const { return equal(retired_, epoch, epochBytes); }
bool SecureLink::confirmedEpoch(const uint8_t* epoch) const {
    return confirmedFrame_ && equal(confirmed_, epoch, epochBytes);
}

void SecureLink::acceptBeacon(const uint8_t* epoch, uint32_t frame) {
    if (confirmedEpoch(epoch)) confirmedFrame_ = frame;
    std::memcpy(epoch_, epoch, epochBytes);
    std::memset(proof_, 0, sizeof(proof_));
    cachedLength_ = 0;
}

bool SecureLink::confirmGrant(uint32_t frame) {
    if (candidateConfirmed()) return false;
    std::memcpy(retired_, confirmed_, epochBytes);
    std::memcpy(confirmed_, epoch_, epochBytes);
    confirmedFrame_ = frame;
    return true;
}

void SecureLink::beginGatewayFrame() { std::memset(requested_, 0, sizeof(requested_)); }

void SecureLink::acceptRequest(uint8_t slot, const uint8_t* challenge) {
    std::memcpy(requested_[slot], challenge, challengeBytes);
}

bool SecureLink::signature(bool sign, const uint8_t* epoch, const uint8_t* bytes, size_t length, uint8_t* tag) {
    uint8_t transcript[2 * epochBytes + maxPacket - signatureBytes];
    if (length > sizeof(transcript) - 2 * epochBytes) return false;
    std::memcpy(transcript, record_ + 12, epochBytes);
    std::memcpy(transcript + epochBytes, epoch, epochBytes);
    std::memcpy(transcript + 2 * epochBytes, bytes, length);
    return sign ? crypto_.sign(transcript, length + 2 * epochBytes, tag) :
        crypto_.verify(transcript, length + 2 * epochBytes, tag);
}

bool SecureLink::authenticateRequest(const uint8_t* master, const uint8_t* bytes, size_t length, uint8_t* tag) {
    uint8_t key[keyBytes]{};
    uint8_t transcript[epochBytes + kHeaderSize + kRequestSize + challengeBytes];
    if (!master || length != sizeof(transcript) - epochBytes) return false;
    std::memcpy(transcript, epoch_, epochBytes);
    std::memcpy(transcript + epochBytes, bytes, length);
    const bool ok = hkdf(master, keyBytes, record_ + 12, epochBytes, requestLabel, sizeof(requestLabel) - 1,
                         key, sizeof(key)) && mac(key, sizeof(key), transcript, sizeof(transcript), tag);
    mbedtls_platform_zeroize(key, sizeof(key));
    return ok;
}

bool SecureLink::protectData(bool encrypt, uint16_t client, const uint8_t* proof, const uint8_t* header,
                             const uint8_t* input, uint8_t* output, size_t length, uint8_t* tag) {
    const auto* master = peerKey(client);
    if (!master || equal(proof, zero, challengeBytes)) return false;
    uint8_t key[keyBytes]{};
    uint8_t salt[2 * epochBytes + challengeBytes];
    std::memcpy(salt, record_ + 12, epochBytes);
    std::memcpy(salt + epochBytes, epoch_, epochBytes);
    std::memcpy(salt + 2 * epochBytes, proof, challengeBytes);
    uint8_t nonce[nonceBytes]{};
    std::memcpy(nonce, header + 12, 12); // Sender boot, frame, sequence and reserved config bytes.
    nonce[12] = wireVersion;
    const bool ok = hkdf(master, keyBytes, salt, sizeof(salt), dataLabel, sizeof(dataLabel) - 1, key, sizeof(key)) &&
        ccm(encrypt, key, nonce, header, kHeaderSize, input, output, length, tag);
    mbedtls_platform_zeroize(key, sizeof(key));
    return ok;
}

bool SecureLink::seal(const Frame& f, const Roster& roster, uint8_t slot, uint8_t* bytes, size_t& length) {
    length = 0;
    if (!population_ || gatewayMessage(f.type) != isGateway()) return false;
    const size_t extra = overhead(f.type, roster.count);
    if (!extra || roster.count > kMaxClients || encode(f, bytes, kMaxFrameSize, length) != CodecResult::OK ||
        extra > maxPacket - length) return false;
    auto* tail = bytes + length;
    if (isGateway()) {
        if (f.type == MessageType::BEACON) std::memcpy(tail, epoch_, epochBytes);
        if (f.type == MessageType::SLOT_GRANT) {
            for (uint8_t i = 0; i < roster.count; ++i)
                std::memcpy(tail + i * echoBytes, requested_[i], echoBytes);
        }
        const size_t body = length + extra - signatureBytes;
        if (!signature(true, epoch_, bytes, body, bytes + body)) return false;
    } else if (f.type == MessageType::PAYLOAD_REQUEST) {
        if (slot >= roster.count || !crypto_.random(proof_, sizeof(proof_)) || equal(proof_, zero, sizeof(proof_)))
            return false;
        std::memcpy(tail, proof_, sizeof(proof_));
        if (!authenticateRequest(peerKey(f.sourceId), bytes, length + sizeof(proof_), tail + sizeof(proof_))) return false;
    } else {
        if (cachedLength_) {
            if (std::memcmp(cachedData_, bytes, kHeaderSize)) return false;
            length = cachedLength_;
            std::memcpy(bytes, cachedData_, length);
            return true;
        }
        if (!protectData(true, f.sourceId, proof_, bytes, f.payload, bytes + kHeaderSize, f.payloadLength, tail))
            return false;
        cachedLength_ = length + extra;
        std::memcpy(cachedData_, bytes, cachedLength_);
    }
    length += extra;
    return true;
}

bool SecureLink::open(const uint8_t* bytes, size_t length, const Roster& roster, uint8_t localSlot,
                      Frame& frame, uint8_t* auxiliary) {
    if (!population_ || !bytes || !auxiliary || length < kHeaderSize || length > maxPacket || bytes[2] != wireVersion ||
        get16(bytes + 6) != config_.networkId || bytes[26] > kMaxPayloadSize || roster.count > kMaxClients)
        return false;
    const auto type = static_cast<MessageType>(bytes[3]);
    const size_t plain = kHeaderSize + bytes[26];
    const size_t extra = overhead(type, roster.count);
    const uint16_t sender = get16(bytes + 8);
    if (!extra || length != plain + extra || gatewayMessage(type) == isGateway()) return false;
    if (!isGateway()) {
        if (sender != config_.gatewayId || decode(bytes, plain, config_.networkId, config_.nodeId, frame) != CodecResult::OK)
            return false;
        if (type == MessageType::SLOT_GRANT && (localSlot >= roster.count ||
            equal(proof_, zero, challengeBytes) || !equal(proof_, bytes + plain + localSlot * echoBytes, echoBytes)))
            return false;
        const auto* epoch = type == MessageType::BEACON ? bytes + plain : epoch_;
        uint8_t signatureBytesCopy[signatureBytes];
        std::memcpy(signatureBytesCopy, bytes + length - signatureBytes, signatureBytes);
        if (equal(epoch, zero, epochBytes) ||
            !signature(false, epoch, bytes, length - signatureBytes, signatureBytesCopy)) return false;
        if (type == MessageType::BEACON) std::memcpy(auxiliary, epoch, epochBytes);
        return true;
    }
    const int slot = roster.indexOf(sender);
    if (slot < 0 || !peerKey(sender)) return false;
    if (type == MessageType::PAYLOAD_REQUEST) {
        uint8_t tag[tagBytes];
        if (decode(bytes, plain, config_.networkId, config_.nodeId, frame) != CodecResult::OK ||
            equal(bytes + plain, zero, challengeBytes) ||
            !authenticateRequest(peerKey(sender), bytes, plain + challengeBytes, tag) ||
            !equal(tag, bytes + plain + challengeBytes, tagBytes)) return false;
        std::memcpy(auxiliary, bytes + plain, challengeBytes);
        return true;
    }
    uint8_t decoded[kMaxFrameSize];
    uint8_t tag[tagBytes];
    std::memcpy(decoded, bytes, kHeaderSize);
    std::memcpy(tag, bytes + plain, tagBytes);
    const bool ok = protectData(false, sender, requested_[slot], bytes, bytes + kHeaderSize,
                                decoded + kHeaderSize, bytes[26], tag) &&
        decode(decoded, plain, config_.networkId, config_.nodeId, frame) == CodecResult::OK;
    mbedtls_platform_zeroize(decoded, sizeof(decoded));
    return ok;
}
}
#endif
