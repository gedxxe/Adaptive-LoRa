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
// Additional TX processing allowance outside the existing radio-only guard.
// Measured on the target before pilot use; late crypto never launches a packet.
constexpr uint32_t cryptoTxBudgetUs = 40000;
}
