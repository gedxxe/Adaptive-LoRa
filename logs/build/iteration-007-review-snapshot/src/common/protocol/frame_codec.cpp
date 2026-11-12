#include "frame_codec.h"
#include <cstring>

namespace adaptive_lora {
namespace {
bool unicast(uint16_t n) { return n != 0 && n != 65535; }
CodecResult validate(const Frame& f) {
    if (f.type != MessageType::HELLO && f.type != MessageType::ACK) return CodecResult::TYPE;
    if (f.payloadLength > kMaxPayloadSize) return CodecResult::LENGTH;
    if (f.networkId == 0) return CodecResult::NETWORK;
    if (!unicast(f.sourceId)) return CodecResult::SOURCE;
    if (!unicast(f.destinationId) || f.destinationId == f.sourceId) return CodecResult::DESTINATION;
    if (f.bootId == 0) return CodecResult::SESSION;
    if (f.frameId || f.configVersion || f.scheduleVersion) return CodecResult::M2_FIELDS;
    if (f.type == MessageType::ACK &&
        (f.payloadLength != kAckPayloadSize || !unicast(get16(f.payload)) ||
         get16(f.payload) != f.destinationId || get32(f.payload + 2) == 0 || f.payload[8] != 0))
        return CodecResult::PAYLOAD;
    return CodecResult::OK;
}
}
const char* codecResultName(CodecResult r) {
    switch (r) {
        case CodecResult::OK: return "ok";
        case CodecResult::BUFFER: return "buffer";
        case CodecResult::MAGIC: return "magic";
        case CodecResult::VERSION: return "version";
        case CodecResult::HEADER: return "header";
        case CodecResult::RESERVED: return "reserved";
        case CodecResult::TYPE: return "type";
        case CodecResult::LENGTH: return "length";
        case CodecResult::NETWORK: return "network";
        case CodecResult::DESTINATION: return "destination";
        case CodecResult::SOURCE: return "source";
        case CodecResult::SESSION: return "session";
        case CodecResult::M2_FIELDS: return "m2_fields";
        case CodecResult::PAYLOAD: return "payload";
    }
    return "unknown";
}
CodecResult encodeFrame(const Frame& f, uint8_t* b, size_t cap, size_t& len) {
    len = 0;
    const CodecResult valid = validate(f);
    if (valid != CodecResult::OK) return valid;
    if (!b || cap < kHeaderSize + f.payloadLength) return CodecResult::BUFFER;
    put16(b, 0x4D4C);
    b[2] = kProtocolVersion;
    b[3] = static_cast<uint8_t>(f.type);
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
    len = kHeaderSize + f.payloadLength;
    return CodecResult::OK;
}
CodecResult decodeFrame(const uint8_t* b, size_t len, uint16_t network,
                        uint16_t local, Frame& out) {
    if (!b) return CodecResult::BUFFER;
    if (len < kHeaderSize || len > kMaxFrameSize) return CodecResult::LENGTH;
    if (get16(b) != 0x4D4C) return CodecResult::MAGIC;
    if (b[2] != kProtocolVersion) return CodecResult::VERSION;
    if (b[5] != kHeaderSize) return CodecResult::HEADER;
    if (b[4] || b[27]) return CodecResult::RESERVED;
    if (b[3] != uint8_t(MessageType::HELLO) && b[3] != uint8_t(MessageType::ACK)) return CodecResult::TYPE;
    if (b[26] > kMaxPayloadSize || len != kHeaderSize + b[26]) return CodecResult::LENGTH;
    if (network == 0 || get16(b + 6) != network) return CodecResult::NETWORK;
    if (!unicast(local) || get16(b + 10) != local) return CodecResult::DESTINATION;
    Frame f;
    f.type = static_cast<MessageType>(b[3]);
    f.networkId = get16(b + 6);
    f.sourceId = get16(b + 8);
    f.destinationId = get16(b + 10);
    f.bootId = get32(b + 12);
    f.frameId = get32(b + 16);
    f.sequence = get16(b + 20);
    f.configVersion = get16(b + 22);
    f.scheduleVersion = get16(b + 24);
    f.payloadLength = b[26];
    std::memcpy(f.payload, b + kHeaderSize, f.payloadLength);
    const CodecResult result = validate(f);
    if (result == CodecResult::OK) out = f;
    return result;
}
void setAckPayload(Frame& ack, const Frame& hello) {
    ack.payloadLength = kAckPayloadSize;
    put16(ack.payload, hello.sourceId);
    put32(ack.payload + 2, hello.bootId);
    put16(ack.payload + 6, hello.sequence);
    ack.payload[8] = 0;
}
bool ackMatches(const Frame& a, const Frame& h, uint16_t gateway) {
    return validate(a) == CodecResult::OK && a.type == MessageType::ACK &&
           a.networkId == h.networkId && a.sourceId == gateway && a.destinationId == h.sourceId &&
           get16(a.payload) == h.sourceId && get32(a.payload + 2) == h.bootId &&
           get16(a.payload + 6) == h.sequence && a.payload[8] == 0;
}
}  // namespace adaptive_lora
