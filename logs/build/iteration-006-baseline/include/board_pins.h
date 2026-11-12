#pragma once

// PROVISIONAL: verify actual board revision, pad labels and continuity before wiring.
namespace adaptive_lora::pins {
constexpr int sck = 4;
constexpr int miso = 5;
constexpr int mosi = 6;
constexpr int nss = 7;
constexpr int reset = 1;
constexpr int dio1 = 0;
constexpr int busy = 10;
}  // namespace adaptive_lora::pins
