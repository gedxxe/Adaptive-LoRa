#pragma once
#include <cstddef>
#include <cstdint>
#include "project_config.h"

namespace adaptive_lora {
enum class DeviceRole : uint8_t { UNSET = 0, GATEWAY = 1, CLIENT = 2 };
struct DeviceConfig {
    uint16_t schema = kConfigSchema;
    DeviceRole role = DeviceRole::UNSET;
    uint16_t networkId = 1;
    uint16_t nodeId = 2;
    uint16_t gatewayId = 1;
    uint32_t helloIntervalMs = kDefaultHelloIntervalMs;
};
const char* roleName(DeviceRole role);
const char* validateConfig(const DeviceConfig& config);  // nullptr means valid.
constexpr size_t kConfigBlobSize = 24;
bool encodeConfig(const DeviceConfig& config, uint8_t* bytes, size_t capacity);
const char* decodeConfig(const uint8_t* bytes, size_t length, DeviceConfig& config);
}  // namespace adaptive_lora
