#pragma once
#include "common/protocol/multiclient_protocol.h"

namespace adaptive_lora::multiclient {
// Small periodic traffic for the adaptive field test. The codec still accepts
// up to kMaxData; the fixed-SF comparison retains its original roster budget.
constexpr uint8_t kBenchPayloadBytes = kAdaptive ? 16 : kMaxData;
constexpr Roster kBenchRoster{kAdaptive ? kAdaptivePeriodUs : 0, 8,
    {{2, kBenchPayloadBytes}, {3, kBenchPayloadBytes}, {4, kBenchPayloadBytes}, {5, kBenchPayloadBytes},
     {6, kBenchPayloadBytes}, {7, kBenchPayloadBytes}, {8, kBenchPayloadBytes}, {9, kBenchPayloadBytes}}};
}
