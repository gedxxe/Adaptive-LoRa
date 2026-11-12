#pragma once
#include <cstdint>

namespace adaptive_lora::phy {
constexpr float frequencyMhz = 915.0f;  // Provisional; local RF review still required.
constexpr float bandwidthKhz = 125.0f;
constexpr uint8_t controlSf = 9;  // Reserved for M3; never selected in M2.
constexpr uint8_t dataSf = 7;
constexpr uint8_t codingRateDenominator = 5;
constexpr uint16_t preambleSymbols = 8;
constexpr uint8_t privateSyncWord = 0x12;  // RadioLib expands the one-byte LoRa word.
constexpr int8_t txPowerDbm = 10;
constexpr uint32_t spiClockHz = 1000000;
constexpr float tcxoVoltage = 0.0f;  // XTAL assumption; verify exact module revision.
constexpr bool useRegulatorLdo = true;  // Conservative bench choice; verify on hardware.
static_assert(dataSf >= 5 && dataSf <= 9 && controlSf <= 9,
              "LLCC68 BW125 supports SF5 through SF9 only");
}  // namespace adaptive_lora::phy
