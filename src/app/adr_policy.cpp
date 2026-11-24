#include "adr_policy.h"
#include <algorithm>
#include <cmath>

namespace adaptive_lora {
namespace {
void clearEvidence(AdrState& state) {
    state.degradedHistory = 0;
    state.missedHistory = 0;
    state.score = 0;
}

bool probingHigher(const AdrState& state) {
    return state.probeReturn && state.desired > state.probeReturn;
}

void clearProbe(AdrState& state) {
    state.probeReturn = 0;
    state.probeStartedFrame = 0;
}

void rollbackProbe(AdrState& state, uint32_t frame, const AdrParams& params) {
    if (probingHigher(state)) {
        state.reservedSf = std::min(state.reservedSf, state.probeReturn);
        state.recoveryFailedFrame = frame;
    }
    state.desired = state.probeReturn;
    clearProbe(state);
    state.consecutiveFirst = 0;
    clearEvidence(state);
    state.hold = params.rollbackHold;
}

bool startRecoveryProbe(AdrState& state, uint8_t candidate, uint32_t frame, const AdrParams& params) {
    if (candidate <= state.desired || candidate > phy::maxDataSf ||
        (state.recoveryFailedFrame && frame - state.recoveryFailedFrame < params.probeReceiptLimit))
        return false;
    state.probeReturn = state.desired;
    state.probeStartedFrame = frame;
    state.desired = candidate;
    state.locked = false;
    clearEvidence(state);
    return true;
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
    if (sf < phy::dataSf || sf > phy::maxDataSf) return kAdrMissingSnr;
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

void resumeAdrAfterIdle(AdrState& state, uint32_t frame, const AdrParams& p) {
    state.desired = state.recoverySf;
    // The most recent accepted DATA is a useful candidate even if its ACK was lost.
    startRecoveryProbe(state, state.deliveredSf, frame, p);
}

bool expireAdrProbe(AdrState& state, uint32_t frame, const AdrParams& p) {
    if (!state.probeReturn || frame - state.probeStartedFrame <= p.probeReceiptLimit) return false;
    rollbackProbe(state, frame, p);
    return true;
}

AdrReview reviewAdr(AdrState& state, AdrRule rule, uint8_t sf, uint32_t frame,
                    const AdrObservation& obs, const AdrParams& p) {
    if (obs.outcome == AdrOutcome::ABSENT || obs.outcome == AdrOutcome::EXCLUDED) return {"observe"};
    if (sf < phy::dataSf || sf > phy::maxDataSf) return {"invalid_sf"};
    const bool legacy = rule == AdrRule::LEGACY;
    const bool first = obs.outcome == AdrOutcome::OK_FIRST;
    const bool primaryMiss = obs.outcome == AdrOutcome::PRIMARY_MISS;
    const bool lost = primaryMiss || obs.outcome == AdrOutcome::LOST_ALL;
    const auto index = sf - phy::dataSf;
    // The Gateway cannot tell whether the Client received a Grant or transmitted DATA.
    const bool weakRequest = std::isfinite(obs.requestSnr) &&
        obs.requestSnr + p.dataOffsetDb[index] - p.requiredSnrDb[index] < p.upMarginDb;
    const bool unconfirmedLoss = lost && (primaryMiss || !weakRequest);
    const bool clientConfirmed = state.acknowledgedSf == sf;
    if (state.observedSf != sf) {
        state.observedSf = sf;
        state.consecutiveFirst = 0;
        state.filteredSnr = kAdrMissingSnr;
        clearEvidence(state);
    }
    if (!legacy) {
        const float sample = std::isfinite(obs.requestSnr) ? obs.requestSnr : obs.dataSnr - p.dataOffsetDb[index];
        state.estimate.update(rule, sample, p);
        const float weight = first || unconfirmedLoss ? 0.0f : lost ? p.lostWeight : p.rescuedWeight;
        state.score = p.decay * state.score + weight;
    }
    if (probingHigher(state)) {
        const bool admitted = sf == state.desired;
        if (admitted && clientConfirmed) {
            clearProbe(state);
            return {"recovery_confirmed"};
        }
        if (!admitted || frame - state.probeStartedFrame >= p.probeReceiptLimit ||
            (lost && sf == phy::maxDataSf)) {
            rollbackProbe(state, frame, p);
            return {admitted ? "recovery_failed" : "recovery_budget", AdrDecision::ROLLBACK};
        }
        if (!lost) return {"recovery_wait_ack"};
        ++state.desired;
        return {"recovery_next", AdrDecision::UP};
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
        (!clientConfirmed && frame - state.probeStartedFrame >= p.probeReceiptLimit))) {
        const float score = state.score;
        rollbackProbe(state, frame, p);
        return {first ? "probe_no_receipt" : "rollback", AdrDecision::ROLLBACK, score};
    }
    const unsigned mask = (1u << p.legacyWindow) - 1u;
    if (legacy)
        state.degradedHistory = uint8_t(((state.degradedHistory << 1) | (!first && !unconfirmedLoss)) & mask);
    state.missedHistory = uint8_t(((state.missedHistory << 1) | lost) & mask);
    if (!first) {
        state.consecutiveFirst = 0;
        const bool weak = !unconfirmedLoss && (legacy ? countMisses(state.degradedHistory) >= p.legacyMisses :
            state.score >= p.upScore && state.estimate.margin(rule, sf, p) < p.upMarginDb);
        if (!weak) {
            // Missing DATA alone only permits a bounded search. A receipt commits the result.
            if (lost && countMisses(state.missedHistory) >= p.legacyMisses &&
                startRecoveryProbe(state, uint8_t(sf + 1), frame, p))
                return {"recovery_probe", AdrDecision::UP};
            if (primaryMiss) return {"primary_loss"};
            return {unconfirmedLoss ? "loss_unconfirmed" : "observe"};
        }
        const float score = state.score;
        state.locked = false;
        clearEvidence(state);
        if (state.desired == phy::maxDataSf) return {"ceiling", AdrDecision::HOLD, score};
        ++state.desired;
        return {"step_up", AdrDecision::UP, score};
    }

    if (legacy && std::isfinite(obs.dataSnr)) {
        state.filteredSnr = std::isfinite(state.filteredSnr) ?
            (1 - p.alpha) * state.filteredSnr + p.alpha * obs.dataSnr : obs.dataSnr;
    }
    if (state.consecutiveFirst < p.firstBeforeProbe) ++state.consecutiveFirst;
    const char* action = "observe";
    if (state.consecutiveFirst >= p.confirmations && !state.locked && clientConfirmed) {
        clearProbe(state);
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
            state.probeStartedFrame = frame;
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
        case AdrOutcome::PRIMARY_MISS: return "PRIMARY_MISS";
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
    }
    return "unknown";
}
}
