#pragma once
#include <cstddef>
#include <cstdint>

namespace adaptive_lora {
constexpr char kFirmwareVersion[] = "0.2.0-alpha.1";
constexpr char kBuildId[] = "m0-m2-source-001";
constexpr uint8_t kProtocolVersion = 1;
constexpr uint16_t kConfigSchema = 1;
constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kDefaultHelloIntervalMs = 5000;
constexpr uint32_t kMinHelloIntervalMs = 1000;
constexpr uint32_t kMaxHelloIntervalMs = 3600000;
constexpr uint32_t kAckTimeoutUs = 1000000;
constexpr uint32_t kTxTimeoutUs = 2000000;
constexpr uint32_t kRadioRecoveryUs = 1000000;
constexpr size_t kMaxFrameSize = 96;
constexpr size_t kHeaderSize = 28;
constexpr size_t kMaxPayloadSize = kMaxFrameSize - kHeaderSize;

// All deadlines used here are less than half the uint32_t microsecond range.
inline bool deadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}
}  // namespace adaptive_lora
