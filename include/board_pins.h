#pragma once

// Heltec WiFi LoRa 32 V3.4. The owner confirms the V3.2 wiring and pin map.
// Electrical reference: Heltec V3.2 schematic and Arduino V3 variant.
namespace adaptive_lora::pins {
constexpr int sck = 9;
constexpr int miso = 11;
constexpr int mosi = 10;
constexpr int nss = 8;
constexpr int reset = 12;
constexpr int dio1 = 14;
constexpr int busy = 13;
}  // namespace adaptive_lora::pins
