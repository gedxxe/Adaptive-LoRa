#pragma once
#include "common/protocol/multiclient_protocol.h"

namespace adaptive_lora::multiclient {
// Reserve the full synthetic application record in every adaptive roster position.
constexpr uint8_t kBenchPayloadBytes = kAdaptive ? 32 : kMaxData;
constexpr Roster kBenchRoster{kAdaptive ? kAdaptivePeriodUs : 0, 8,
    {{2, kBenchPayloadBytes}, {3, kBenchPayloadBytes}, {4, kBenchPayloadBytes}, {5, kBenchPayloadBytes},
     {6, kBenchPayloadBytes}, {7, kBenchPayloadBytes}, {8, kBenchPayloadBytes}, {9, kBenchPayloadBytes}}};
}
