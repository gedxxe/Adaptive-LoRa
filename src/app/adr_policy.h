#pragma once
#include <limits>
#include "radio_config.h"

namespace adaptive_lora {
enum class AdrRule : uint8_t { LEGACY = 0, MARGIN_EWMA = 1, MARGIN_KALMAN = 2 };
enum class AdrOutcome : uint8_t { ABSENT, OK_FIRST, RESCUED_SAME_SF, LOST_ALL, EXCLUDED };
enum class AdrDecision : uint8_t { HOLD, UP, DOWN, ROLLBACK, OVERRIDE };

struct AdrParams {
    uint8_t version = 1;
    uint8_t legacyWindow = 3;
    uint8_t legacyMisses = 2;
    uint8_t confirmations = 3;
    uint8_t firstBeforeProbe = 6;
    uint8_t probeReceiptLimit = 6;
    uint8_t rollbackHold = 8;
    uint8_t idleFrames = 4;
    uint8_t hardLosses = 4;
    float alpha = 0.25f;
    float decay = 0.75f;
    float lostWeight = 1.0f;
    float rescuedWeight = 0.4f;
    float upScore = 1.5f;
    float upMarginDb = 0.0f;
    float downMarginDb = 4.0f;
    float hysteresisDb = 2.0f;
    float uncertaintyZ = 1.0f;
    float qLevel = 0.16f;
    float qSlope = 0.01f;
    float measurementVariance = 1.0f;
    float initialSlopeVariance = 0.25f;
    // Provisional thresholds. Replace with measured SNR_50 for SF7 through SF10.
    float requiredSnrDb[phy::maxDataSf - phy::dataSf + 1] = {-7.5f, -10.0f, -12.5f, -15.0f};
    // Expected DATA SNR minus the SF9 Request SNR, indexed by DATA SF.
    float dataOffsetDb[phy::maxDataSf - phy::dataSf + 1] = {};
    float legacyProbeSnrDb[phy::maxDataSf - phy::dataSf] = {-3.5f, -6.0f, -8.5f};
};

constexpr AdrParams kAdrParams{};
static_assert(kAdrParams.legacyWindow <= 8 && kAdrParams.legacyMisses <= kAdrParams.legacyWindow,
              "The legacy window must fit in one byte");
static_assert(kAdrParams.downMarginDb - kAdrParams.upMarginDb >= kAdrParams.hysteresisDb,
              "The ADR margins must leave room for hysteresis");
#if defined(ADAPTIVE_LORA_ADR_RULE)
static_assert(ADAPTIVE_LORA_ADR_RULE >= 0 && ADAPTIVE_LORA_ADR_RULE <= 2, "Unknown ADR rule");
constexpr auto kFirmwareAdrRule = static_cast<AdrRule>(ADAPTIVE_LORA_ADR_RULE);
#else
constexpr auto kFirmwareAdrRule = AdrRule::LEGACY;
#endif
constexpr float kAdrMissingSnr = std::numeric_limits<float>::quiet_NaN();

struct AdrObservation {
    AdrOutcome outcome;
    float requestSnr = kAdrMissingSnr;
    float dataSnr = kAdrMissingSnr;
};

struct AdrEstimate {
    bool ready = false;
    float level = 0;
    float slope = 0;
    float variance = 0;
    float covariance = 0;
    float slopeVariance = 0;

    void update(AdrRule rule, float sample, const AdrParams& params);
    float sigma(AdrRule rule, const AdrParams& params) const;
    float margin(AdrRule rule, uint8_t sf, const AdrParams& params) const;
};

struct AdrState {
    uint32_t boot = 0;
    uint32_t lastRequest = 0;
    uint32_t deliveredFrame = 0;
    uint8_t desired = phy::dataSf;
    uint8_t reservedSf = phy::dataSf;
    uint8_t recoverySf = phy::dataSf;
    uint8_t observedSf = 0;
    uint8_t deliveredSf = 0;
    uint8_t acknowledgedSf = 0;
    uint8_t consecutiveFirst = 0;
    uint8_t degradedHistory = 0;
    uint8_t probeReturn = 0;
    uint8_t hold = 0;
    bool locked = false;
    float filteredSnr = kAdrMissingSnr;
    AdrEstimate estimate;
    float score = 0;
    uint8_t consecutiveLost = 0;
};

struct AdrReview {
    const char* action;
    AdrDecision decision = AdrDecision::HOLD;
    float decisionScore = kAdrMissingSnr;
};

AdrOutcome classifyAdr(bool requested, bool usable, bool accepted, bool retried);
AdrReview reviewAdr(AdrState& state, AdrRule rule, uint8_t observedSf,
                    const AdrObservation& observation, const AdrParams& params = kAdrParams);
const char* adrRuleName(AdrRule rule);
const char* adrOutcomeName(AdrOutcome outcome);
const char* adrDecisionName(AdrDecision decision);
}
