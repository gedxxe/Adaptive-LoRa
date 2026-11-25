// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>

namespace adaptive_lora::secure {
constexpr uint8_t wireVersion = 7;
constexpr size_t keyBytes = 16;
constexpr size_t tagBytes = 16;
constexpr size_t challengeBytes = 16;
constexpr size_t echoBytes = 12;
constexpr size_t signatureBytes = 64; // P-256 r and s, each 32 bytes, big endian.
constexpr size_t publicKeyBytes = 65; // SEC1 uncompressed point.
constexpr size_t privateKeyBytes = 32;
constexpr size_t epochBytes = 16;
constexpr size_t maxPacket = 237; // 28 + (1 + 6*8) + 12*8 + 64.
// Symmetric Request and DATA allowance, per side.
constexpr uint32_t cryptoTxBudgetUs = 40000;
// P-256 signing took 121.595 ms on the Heltec Gateway (ERR-075).
// Round the observed time plus 50% headroom up to the next 40 ms.
constexpr uint32_t signingBudgetUs = 200000;
// Verification can require two scalar operations. Use a separate budget,
// with headroom over twice the observed signing time pending Client data.
constexpr uint32_t verificationBudgetUs = 300000;
}
