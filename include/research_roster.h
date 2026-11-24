#pragma once
#include "common/protocol/multiclient_protocol.h"

namespace adaptive_lora::multiclient {
// Reserve the full synthetic application record in every adaptive roster position.
constexpr uint8_t kBenchPayloadBytes = kAdaptive ? 32 : kMaxData;
constexpr Roster kBenchRoster{kAdaptive ? kAdaptivePeriodUs : 0, 8,
    {{2, kBenchPayloadBytes}, {3, kBenchPayloadBytes}, {4, kBenchPayloadBytes}, {5, kBenchPayloadBytes},
     {6, kBenchPayloadBytes}, {7, kBenchPayloadBytes}, {8, kBenchPayloadBytes}, {9, kBenchPayloadBytes}}};
#if defined(ADAPTIVE_LORA_ROTATING)
constexpr uint16_t kRegisteredCount = ADAPTIVE_LORA_REGISTERED_CLIENTS;
static_assert(kRegisteredCount > 0 && kRegisteredCount <= kPopulationLimit,
              "Registered population must fit the Gateway registry");
#endif
}
