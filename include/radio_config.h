#pragma once
#include <cstdint>

namespace adaptive_lora::phy {
constexpr float frequencyMhz = 915.0f;  // Provisional; local RF review still required.
constexpr float bandwidthKhz = 125.0f;
constexpr uint8_t controlSf = 9;  // Shared Beacon, Request, Grant and bitmap in the adaptive image.
constexpr uint8_t dataSf = 7;
constexpr uint8_t maxDataSf = 10;
constexpr uint8_t codingRateDenominator = 5;
constexpr uint16_t preambleSymbols = 8;
constexpr uint8_t privateSyncWord = 0x12;  // RadioLib expands the one-byte LoRa word.
constexpr int8_t txPowerDbm = 22;
constexpr float txCurrentLimitMa = 140.0f;  // SX1262 high-power PA OCP setting.
constexpr uint32_t spiClockHz = 1000000;
constexpr float tcxoVoltage = 1.8f;  // DIO3-powered TCXO on the V3.2 schematic; vendor uses 1.8 V.
constexpr bool useRegulatorLdo = false;  // V3.2 populates the SX1262 DC-DC network.
}  // namespace adaptive_lora::phy
