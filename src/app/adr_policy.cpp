#include "adr_policy.h"
#include <algorithm>
#include <cmath>

namespace adaptive_lora {
namespace {
void clearEvidence(AdrState& state) {
    state.degradedHistory = 0;
    state.score = 0;
    state.consecutiveLost = 0;
}

uint8_t countMisses(uint8_t mask) {
    uint8_t count = 0;
    for (; mask; mask &= uint8_t(mask - 1)) ++count;
    return count;
}
}

void AdrEstimate::update(AdrRule rule, float sample, const AdrParams& p) {
    const bool measured = std::isfinite(sample);
    if (!ready) {
        if (measured) {
            ready = true;
            level = sample;
            variance = p.measurementVariance;
            slopeVariance = p.initialSlopeVariance;
        }
        return;
    }
    if (rule == AdrRule::MARGIN_EWMA) {
        if (measured) {
            const float residual = sample - level;
            level += p.alpha * residual;
            variance = (1 - p.alpha) * (variance + p.alpha * residual * residual);
        }
        return;
    }
    // Local linear trend with one step per eligible report and a level-only measurement.
    level += slope;
    variance += 2 * covariance + slopeVariance + p.qLevel;
    covariance += slopeVariance;
    slopeVariance += p.qSlope;
    if (!measured) return;
    const float residual = sample - level;
    const float innovationVariance = variance + p.measurementVariance;
    const float ratio = p.measurementVariance / innovationVariance;
    level += (variance / innovationVariance) * residual;
    slope += (covariance / innovationVariance) * residual;
    slopeVariance = std::max(0.0f, slopeVariance - covariance * covariance / innovationVariance);
    variance *= ratio;
    covariance *= ratio;
}

float AdrEstimate::sigma(AdrRule rule, const AdrParams& p) const {
    const float nextVariance = rule == AdrRule::MARGIN_KALMAN ?
        variance + 2 * covariance + slopeVariance + p.qLevel : variance;
    return ready ? std::sqrt(std::max(0.0f, nextVariance)) : kAdrMissingSnr;
}

float AdrEstimate::margin(AdrRule rule, uint8_t sf, const AdrParams& p) const {
    const auto index = sf - phy::dataSf;
    const float predicted = rule == AdrRule::MARGIN_KALMAN ? level + slope : level;
    return ready ? predicted + p.dataOffsetDb[index] - p.requiredSnrDb[index] - p.uncertaintyZ * sigma(rule, p) : kAdrMissingSnr;
}

AdrOutcome classifyAdr(bool requested, bool usable, bool accepted, bool retried) {
    if (!requested) return AdrOutcome::ABSENT;
    if (!usable) return AdrOutcome::EXCLUDED;
    if (!accepted) return AdrOutcome::LOST_ALL;
    return retried ? AdrOutcome::RESCUED_SAME_SF : AdrOutcome::OK_FIRST;
}

AdrReview reviewAdr(AdrState& state, AdrRule rule, uint8_t sf, const AdrObservation& obs, const AdrParams& p) {
    if (obs.outcome == AdrOutcome::ABSENT || obs.outcome == AdrOutcome::EXCLUDED) return {"observe"};
    const bool legacy = rule == AdrRule::LEGACY;
    const bool first = obs.outcome == AdrOutcome::OK_FIRST;
    const bool clientConfirmed = state.acknowledgedSf == sf;
    if (state.observedSf != sf) {
        state.observedSf = sf;
        state.consecutiveFirst = 0;
        state.filteredSnr = kAdrMissingSnr;
        clearEvidence(state);
    }
    if (!legacy) {
        const float sample = std::isfinite(obs.requestSnr) ? obs.requestSnr : obs.dataSnr - p.dataOffsetDb[sf - phy::dataSf];
        state.estimate.update(rule, sample, p);
        const bool lost = obs.outcome == AdrOutcome::LOST_ALL;
        state.score = p.decay * state.score + (first ? 0.0f : lost ? p.lostWeight : p.rescuedWeight);
        if (!lost) state.consecutiveLost = 0;
        else if (state.consecutiveLost < p.hardLosses) ++state.consecutiveLost;
    }
    if (sf != state.desired) {
        if (!first) state.consecutiveFirst = 0;
        else {
            if (state.consecutiveFirst < p.confirmations) ++state.consecutiveFirst;
            if (state.consecutiveFirst >= p.confirmations && clientConfirmed) {
                state.desired = sf;
                state.recoverySf = sf;
                state.locked = true;
                return {"locked_within_budget"};
            }
        }
        return {"budget_limited"};
    }
    if (state.probeReturn && (!first ||
        (!clientConfirmed && state.consecutiveFirst + 1 >= p.probeReceiptLimit))) {
        const float score = state.score;
        state.desired = state.probeReturn;
        state.probeReturn = 0;
        state.consecutiveFirst = 0;
        clearEvidence(state);
        state.hold = p.rollbackHold;
        return {first ? "probe_no_receipt" : "rollback", AdrDecision::ROLLBACK, score};
    }
    if (legacy) {
        const unsigned mask = (1u << p.legacyWindow) - 1u;
        state.degradedHistory = uint8_t(((state.degradedHistory << 1) | !first) & mask);
    }
    if (!first) {
        state.consecutiveFirst = 0;
        const bool override = !legacy && state.consecutiveLost >= p.hardLosses;
        const bool weak = legacy ? countMisses(state.degradedHistory) >= p.legacyMisses :
            state.score >= p.upScore && state.estimate.margin(rule, sf, p) < p.upMarginDb;
        if (!override && !weak) return {"observe"};
        const float score = state.score;
        state.locked = false;
        clearEvidence(state);
        if (state.desired == phy::maxDataSf) return {"ceiling", AdrDecision::HOLD, score};
        ++state.desired;
        return override ? AdrReview{"hard_override", AdrDecision::OVERRIDE, score} :
                          AdrReview{"step_up", AdrDecision::UP, score};
    }

    if (legacy && std::isfinite(obs.dataSnr)) {
        state.filteredSnr = std::isfinite(state.filteredSnr) ?
            (1 - p.alpha) * state.filteredSnr + p.alpha * obs.dataSnr : obs.dataSnr;
    }
    if (state.consecutiveFirst < p.firstBeforeProbe) ++state.consecutiveFirst;
    const char* action = "observe";
    if (state.consecutiveFirst >= p.confirmations && !state.locked && clientConfirmed) {
        state.probeReturn = 0;
        state.reservedSf = state.desired;
        state.recoverySf = sf;
        state.locked = true;
        action = "locked";
    }
    if (state.hold) --state.hold;
    else if (state.locked && clientConfirmed && sf > phy::dataSf && state.consecutiveFirst >= p.firstBeforeProbe) {
        const bool enoughMargin = legacy ? state.filteredSnr >= p.legacyProbeSnrDb[sf - phy::dataSf - 1] :
            state.estimate.margin(rule, uint8_t(sf - 1), p) >= p.downMarginDb;
        if (enoughMargin) {
            state.probeReturn = state.desired--;
            state.consecutiveFirst = 0;
            state.locked = false;
            return {"probe_lower", AdrDecision::DOWN};
        }
    }
    return {action};
}

const char* adrRuleName(AdrRule rule) {
    switch (rule) {
        case AdrRule::LEGACY: return "LEGACY";
        case AdrRule::MARGIN_EWMA: return "MARGIN_EWMA";
        case AdrRule::MARGIN_KALMAN: return "MARGIN_KALMAN";
    }
    return "UNKNOWN";
}

const char* adrOutcomeName(AdrOutcome outcome) {
    switch (outcome) {
        case AdrOutcome::ABSENT: return "ABSENT";
        case AdrOutcome::OK_FIRST: return "OK_FIRST";
        case AdrOutcome::RESCUED_SAME_SF: return "RESCUED_SAME_SF";
        case AdrOutcome::LOST_ALL: return "LOST_ALL";
        case AdrOutcome::EXCLUDED: return "EXCLUDED";
    }
    return "UNKNOWN";
}

const char* adrDecisionName(AdrDecision decision) {
    switch (decision) {
        case AdrDecision::HOLD: return "hold";
        case AdrDecision::UP: return "up";
        case AdrDecision::DOWN: return "down";
        case AdrDecision::ROLLBACK: return "rollback";
        case AdrDecision::OVERRIDE: return "override";
    }
    return "unknown";
}
}
