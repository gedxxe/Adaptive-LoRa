#pragma once
#include "common/protocol/multiclient_protocol.h"

namespace adaptive_lora::multiclient {
// Gateway admission order. Replace this table when building a different roster.
// Zero selects the shortest period that fits the configured payload budgets.
constexpr Roster kBenchRoster{0, 8, {{2, 64}, {3, 64}, {4, 64}, {5, 64},
                                     {6, 64}, {7, 64}, {8, 64}, {9, 64}}};
}
