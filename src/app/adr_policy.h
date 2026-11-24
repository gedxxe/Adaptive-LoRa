#pragma once
#include <limits>
#include "radio_config.h"

namespace adaptive_lora {
enum class AdrRule : uint8_t { LEGACY = 0, MARGIN_EWMA = 1, MARGIN_KALMAN = 2 };
enum class AdrOutcome : uint8_t { ABSENT, OK_FIRST, RESCUED_SAME_SF, LOST_ALL, PRIMARY_MISS, EXCLUDED };
enum class AdrDecision : uint8_t { HOLD, UP, DOWN, ROLLBACK };

struct AdrParams {
    uint8_t version = 5;
    uint8_t legacyWindow = 3;
    uint8_t legacyMisses = 2;
    uint8_t confirmations = 3;
    uint8_t firstBeforeProbe = 6;
    uint8_t probeReceiptLimit = 6;
    uint8_t rollbackHold = 8;
    uint8_t idleFrames = 4;
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

constexpr bool finiteAdrValue(float value) {
    return value >= -std::numeric_limits<float>::max() && value <= std::numeric_limits<float>::max();
}

constexpr bool validAdrParams(const AdrParams& p) {
    if (!p.legacyMisses || p.legacyMisses > p.legacyWindow || p.legacyWindow > 8 ||
        !p.confirmations || p.confirmations > p.firstBeforeProbe || p.confirmations > p.probeReceiptLimit ||
        p.probeReceiptLimit <= phy::maxDataSf - phy::dataSf ||
        !p.idleFrames || !(p.alpha > 0 && p.alpha <= 1) || !(p.decay >= 0 && p.decay < 1) ||
        !finiteAdrValue(p.lostWeight) || !(p.rescuedWeight >= 0 && p.rescuedWeight <= p.lostWeight) ||
        !(p.upScore > 0 && p.upScore * (1 - p.decay) < p.lostWeight) ||
        !finiteAdrValue(p.upMarginDb) || !finiteAdrValue(p.downMarginDb) ||
        !finiteAdrValue(p.hysteresisDb) || !(p.hysteresisDb > 0 && p.downMarginDb - p.upMarginDb >= p.hysteresisDb))
        return false;
    if (!finiteAdrValue(p.uncertaintyZ) || p.uncertaintyZ < 0 ||
        !finiteAdrValue(p.qLevel) || p.qLevel < 0 || !finiteAdrValue(p.qSlope) || p.qSlope < 0 ||
        !finiteAdrValue(p.initialSlopeVariance) || p.initialSlopeVariance < 0 ||
        !finiteAdrValue(p.measurementVariance) || p.measurementVariance <= 0)
        return false;
    for (unsigned i = 0; i <= phy::maxDataSf - phy::dataSf; ++i)
        if (!finiteAdrValue(p.requiredSnrDb[i] - p.dataOffsetDb[i]) ||
            (i && p.requiredSnrDb[i] - p.dataOffsetDb[i] > p.requiredSnrDb[i - 1] - p.dataOffsetDb[i - 1]))
            return false;
    for (float threshold : p.legacyProbeSnrDb)
        if (!finiteAdrValue(threshold)) return false;
    return true;
}
static_assert(validAdrParams(kAdrParams), "Invalid ADR parameters");
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
    uint32_t probeStartedFrame = 0;
    uint32_t recoveryFailedFrame = 0;
    uint8_t desired = phy::dataSf;
    uint8_t reservedSf = phy::dataSf;
    uint8_t recoverySf = phy::dataSf;
    uint8_t observedSf = 0;
    uint8_t deliveredSf = 0;
    uint8_t acknowledgedSf = 0;
    uint8_t consecutiveFirst = 0;
    uint8_t degradedHistory = 0;
    uint8_t missedHistory = 0;
    uint8_t probeReturn = 0;
    uint8_t hold = 0;
    bool locked = false;
    float filteredSnr = kAdrMissingSnr;
    AdrEstimate estimate;
    float score = 0;
};

struct AdrReview {
    const char* action;
    AdrDecision decision = AdrDecision::HOLD;
    float decisionScore = kAdrMissingSnr;
};

AdrOutcome classifyAdr(bool requested, bool usable, bool accepted, bool retried);
void resumeAdrAfterIdle(AdrState& state, uint32_t frame, const AdrParams& params = kAdrParams);
bool applyAdrReceipt(AdrState& state, uint32_t receipt);
bool expireAdrProbe(AdrState& state, uint32_t frame, const AdrParams& params = kAdrParams);
AdrReview reviewAdr(AdrState& state, AdrRule rule, uint8_t observedSf, uint32_t frame,
                    const AdrObservation& observation, const AdrParams& params = kAdrParams);
const char* adrRuleName(AdrRule rule);
const char* adrOutcomeName(AdrOutcome outcome);
const char* adrDecisionName(AdrDecision decision);
}
