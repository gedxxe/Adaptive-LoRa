#pragma once
#include <cstddef>
#include <cstdint>
#include "message_types.h"
#include "project_config.h"

namespace adaptive_lora {
inline void put16(uint8_t* b, uint16_t n) { b[0] = n; b[1] = n >> 8; }
inline void put32(uint8_t* b, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) b[i] = static_cast<uint8_t>(n >> (8 * i));
}
inline uint16_t get16(const uint8_t* b) { return uint16_t(b[0]) | (uint16_t(b[1]) << 8); }
inline uint32_t get32(const uint8_t* b) {
    return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}
struct Frame {
    MessageType type = MessageType::HELLO;
    uint16_t networkId = 0;
    uint16_t sourceId = 0;
    uint16_t destinationId = 0;
    uint32_t bootId = 0;
    uint32_t frameId = 0;
    uint16_t sequence = 0;
    uint16_t configVersion = 0;
    uint16_t scheduleVersion = 0;
    uint8_t payloadLength = 0;
    uint8_t payload[kMaxPayloadSize]{};
};
enum class CodecResult {
    OK, BUFFER, MAGIC, VERSION, HEADER, RESERVED, TYPE, LENGTH,
    NETWORK, DESTINATION, SOURCE, SESSION, M2_FIELDS, PAYLOAD
};
const char* codecResultName(CodecResult result);
CodecResult encodeFrame(const Frame& frame, uint8_t* bytes, size_t capacity, size_t& length);
CodecResult decodeFrame(const uint8_t* bytes, size_t length, uint16_t networkId,
                        uint16_t localId, Frame& frame);
constexpr uint8_t kAckPayloadSize = 9;
void setAckPayload(Frame& ack, const Frame& hello);
bool ackMatches(const Frame& ack, const Frame& hello, uint16_t gatewayId);
}  // namespace adaptive_lora
