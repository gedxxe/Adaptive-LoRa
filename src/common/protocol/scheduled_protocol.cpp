#include "scheduled_protocol.h"
#include "radio_config.h"
#include <cstring>

namespace adaptive_lora::scheduled {
static_assert(phy::dataSf == 7 && phy::bandwidthKhz == 125 && phy::codingRateDenominator == 5 &&
              phy::preambleSymbols == 8, "Update the scheduled airtime calculation if the PHY changes");

bool validBitmap(uint8_t count, const uint8_t* bits, size_t length) {
    if (!bits || !count || count > 70 || length != (size_t(count) + 7) / 8) return false;
    const unsigned used = count % 8;
    return !used || (bits[length - 1] >> used) == 0;
}

namespace {
CodecResult validate(const Frame& f) {
    if (!f.networkId) return CodecResult::NETWORK;
    if (!f.sourceId || f.sourceId == UINT16_MAX) return CodecResult::SOURCE;
    if (!f.destinationId || f.destinationId == UINT16_MAX || f.destinationId == f.sourceId)
        return CodecResult::DESTINATION;
    if (!f.bootId || !f.frameId) return CodecResult::SESSION;
    if (f.configVersion || f.scheduleVersion != kScheduleVersion) return CodecResult::RESERVED;
    if (f.payloadLength > kMaxPayloadSize) return CodecResult::LENGTH;
    const auto* p = f.payload;
    switch (f.type) {
        case MessageType::BEACON: {
            if (f.sequence || f.payloadLength != kBeaconSize || p[5] != kAttempts) return CodecResult::PAYLOAD;
            const Schedule schedule{get32(p), p[4]};
            return schedule.valid() ? CodecResult::OK : CodecResult::PAYLOAD;
        }
        case MessageType::DATA:
            return f.payloadLength >= kDataPrefix && get32(p) && f.sequence == uint16_t(f.frameId)
                ? CodecResult::OK : CodecResult::PAYLOAD;
        case MessageType::ACK_BITMAP:
            if (f.sequence >= kAttempts || f.payloadLength != kFeedbackSize || p[0] != 1 || p[1] != 1 ||
                !validBitmap(p[0], p + 2, p[1])) return CodecResult::PAYLOAD;
            if (p[2]) return get32(p + 3) && get16(p + 7) == uint16_t(f.frameId)
                ? CodecResult::OK : CodecResult::PAYLOAD;
            return !get32(p + 3) && !get16(p + 7) ? CodecResult::OK : CodecResult::PAYLOAD;
        default: return CodecResult::TYPE;
    }
}
}

CodecResult encode(const Frame& f, uint8_t* b, size_t capacity, size_t& length) {
    length = 0;
    const auto result = validate(f);
    if (result != CodecResult::OK) return result;
    if (!b || capacity < kHeaderSize + f.payloadLength) return CodecResult::BUFFER;
    put16(b, 0x4D4C);
    b[2] = kVersion;
    b[3] = uint8_t(f.type);
    b[4] = 0;
    b[5] = kHeaderSize;
    put16(b + 6, f.networkId);
    put16(b + 8, f.sourceId);
    put16(b + 10, f.destinationId);
    put32(b + 12, f.bootId);
    put32(b + 16, f.frameId);
    put16(b + 20, f.sequence);
    put16(b + 22, f.configVersion);
    put16(b + 24, f.scheduleVersion);
    b[26] = f.payloadLength;
    b[27] = 0;
    std::memcpy(b + kHeaderSize, f.payload, f.payloadLength);
    length = kHeaderSize + f.payloadLength;
    return CodecResult::OK;
}

CodecResult decode(const uint8_t* b, size_t length, uint16_t network, uint16_t local, Frame& out) {
    if (!b) return CodecResult::BUFFER;
    if (length < kHeaderSize || length > kMaxFrameSize || length != kHeaderSize + b[26]) return CodecResult::LENGTH;
    if (get16(b) != 0x4D4C) return CodecResult::MAGIC;
    if (b[2] != kVersion) return CodecResult::VERSION;
    if (b[5] != kHeaderSize) return CodecResult::HEADER;
    if (b[4] || b[27]) return CodecResult::RESERVED;
    if (get16(b + 6) != network) return CodecResult::NETWORK;
    if (get16(b + 10) != local) return CodecResult::DESTINATION;
    Frame f;
    f.type = static_cast<MessageType>(b[3]);
    f.networkId = network;
    f.sourceId = get16(b + 8);
    f.destinationId = local;
    f.bootId = get32(b + 12);
    f.frameId = get32(b + 16);
    f.sequence = get16(b + 20);
    f.configVersion = get16(b + 22);
    f.scheduleVersion = get16(b + 24);
    f.payloadLength = b[26];
    std::memcpy(f.payload, b + kHeaderSize, f.payloadLength);
    const auto result = validate(f);
    if (result == CodecResult::OK) out = f;
    return result;
}
}
