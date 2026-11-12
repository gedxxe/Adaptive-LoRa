#include "device_config.h"
#include "common/protocol/frame_codec.h"

namespace adaptive_lora {
namespace {
uint32_t checksum(const uint8_t* bytes, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
}
const char* roleName(DeviceRole role) {
    switch (role) {
        case DeviceRole::GATEWAY: return "GATEWAY";
        case DeviceRole::CLIENT: return "CLIENT";
        default: return "UNSET";
    }
}
const char* validateConfig(const DeviceConfig& c) {
    if (c.schema != kConfigSchema) return "unsupported_schema";
    if (c.role != DeviceRole::GATEWAY && c.role != DeviceRole::CLIENT) return "invalid_role";
    if (c.networkId == 0) return "invalid_network_id";
    if (c.nodeId == 0 || c.nodeId == 65535) return "invalid_node_id";
    if (c.gatewayId == 0 || c.gatewayId == 65535) return "invalid_gateway_id";
    if (c.role == DeviceRole::GATEWAY && c.nodeId != c.gatewayId) return "gateway_id_mismatch";
    if (c.role == DeviceRole::CLIENT && c.nodeId == c.gatewayId) return "client_id_conflict";
    if (c.helloIntervalMs < kMinHelloIntervalMs || c.helloIntervalMs > kMaxHelloIntervalMs)
        return "invalid_hello_interval";
    return nullptr;
}
bool encodeConfig(const DeviceConfig& c, uint8_t* b, size_t cap) {
    if (!b || cap < kConfigBlobSize || validateConfig(c)) return false;
    put32(b, 0x4344464Du);
    put16(b + 4, c.schema);
    b[6] = static_cast<uint8_t>(c.role);
    b[7] = 0;
    put16(b + 8, c.networkId);
    put16(b + 10, c.nodeId);
    put16(b + 12, c.gatewayId);
    put16(b + 14, 0);
    put32(b + 16, c.helloIntervalMs);
    put32(b + 20, checksum(b, 20));
    return true;
}
const char* decodeConfig(const uint8_t* b, size_t len, DeviceConfig& out) {
    if (!b || len != kConfigBlobSize) return "config_length";
    if (get32(b) != 0x4344464Du || b[7] != 0 || get16(b + 14) != 0 ||
        checksum(b, 20) != get32(b + 20)) return "config_corrupt";
    DeviceConfig c;
    c.schema = get16(b + 4);
    c.role = static_cast<DeviceRole>(b[6]);
    c.networkId = get16(b + 8);
    c.nodeId = get16(b + 10);
    c.gatewayId = get16(b + 12);
    c.helloIntervalMs = get32(b + 16);
    if (const char* error = validateConfig(c)) return error;
    out = c;  // Failed loads leave the caller's safe state untouched.
    return nullptr;
}
}  // namespace adaptive_lora
