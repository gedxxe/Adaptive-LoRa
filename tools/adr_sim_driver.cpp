#include "app/adr_policy.h"
#include "common/protocol/multiclient_protocol.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>

namespace {
using namespace adaptive_lora;
using multiclient::kAdaptivePeriodUs;
using multiclient::kAttempts;
constexpr double secondsPerFrame = kAdaptivePeriodUs / 1000000.0;
constexpr float healthySnr = -2.0f;
constexpr float blockageDb = -9.0f;
constexpr float decodeTransitionDb = 0.75f;
// Model assumptions, independent of the firmware's tunable thresholds.
constexpr float channelSnr50Db[] = {-7.5f, -10.0f, -12.5f, -15.0f};
static_assert(sizeof(channelSnr50Db) / sizeof(float) == phy::maxDataSf - phy::dataSf + 1,
              "Define the modeled channel for each DATA SF");

struct Counts {
    unsigned above = 0;
    unsigned lost = 0;
    unsigned changes = 0;
    unsigned reactions = 0;
    unsigned reactionFrames = 0;
};

void run(Counts& counts, uint32_t seed, AdrRule rule, unsigned frames, float interference, bool blockage,
         const AdrParams& params) {
    std::mt19937 generator(seed);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::normal_distribution<float> measurementNoise(0.0f, 1.0f);
    AdrState state;
    uint8_t lastSf = phy::dataSf;
    uint8_t lastDeliveredSf = 0;
    bool reacted = false;
    for (unsigned frame = 0; frame < frames; ++frame) {
        if (blockage && frame == frames / 3) {
            // Compare recovery from the same SF and state when attenuation begins.
            state = {};
            lastDeliveredSf = 0;
        }
        expireAdrProbe(state, frame + 1, params);
        const bool blocked = blockage && frame >= frames / 3 && frame < 2 * frames / 3;
        const float trueSnr = healthySnr + (blocked ? blockageDb : 0.0f);
        const float requestSnr = trueSnr + measurementNoise(generator);
        const float dataSnr = trueSnr + measurementNoise(generator);
        uint8_t necessary = phy::dataSf;
        while (necessary < phy::maxDataSf && trueSnr < channelSnr50Db[necessary - phy::dataSf])
            ++necessary;
        // One Client owns all its scheduled retries and every supported SF fits.
        const uint8_t sf = state.desired;
        state.reservedSf = std::max(state.reservedSf, sf);
        counts.above += sf > necessary;
        counts.changes += sf != lastSf;
        lastSf = sf;
        if (blocked && !reacted && sf >= necessary) {
            ++counts.reactions;
            counts.reactionFrames += frame - frames / 3;
            reacted = true;
        }
        const float margin = trueSnr - channelSnr50Db[sf - phy::dataSf];
        const float decodeChance = 1.0f / (1.0f + std::exp(-margin / decodeTransitionDb));
        unsigned success = kAttempts;
        // Draw every attempt to keep the same random stream across all three policies.
        for (unsigned attempt = 0; attempt < kAttempts; ++attempt) {
            const float overlap = chance(generator);
            const float decode = chance(generator);
            if (success == kAttempts && overlap >= interference && decode < decodeChance) success = attempt;
        }
        const bool delivered = success < kAttempts;
        counts.lost += !delivered;
        state.acknowledgedSf = lastDeliveredSf;
        reviewAdr(state, rule, sf, frame + 1, {classifyAdr(true, true, delivered, success != 0),
                                             requestSnr, delivered ? dataSnr : kAdrMissingSnr}, params);
        if (delivered) lastDeliveredSf = sf;
    }
}
}

int main(int argc, char** argv) {
    if (argc != 5) return 2;
    const auto seed = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10));
    const auto trials = static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10));
    const auto frames = static_cast<unsigned>(std::strtoul(argv[3], nullptr, 10));
    const float policyShift = std::strtof(argv[4], nullptr);
    AdrParams params = kAdrParams;
    for (float& threshold : params.requiredSnrDb) threshold += policyShift;
    if (!trials || frames < 6 || !validAdrParams(params)) return 2;
    const auto observations = double(frames) * trials;
    std::cout << "scenario,attempt_loss_pct,rule,above_required_pct,final_loss_pct,"
                 "mean_reaction_s,reacted_trials,sf_changes_per_hour\n";
    for (bool blockage : {false, true}) {
        for (unsigned rate : {1, 3, 5, 10, 20}) {
            for (AdrRule rule : {AdrRule::LEGACY, AdrRule::MARGIN_EWMA, AdrRule::MARGIN_KALMAN}) {
                Counts counts;
                for (unsigned trial = 0; trial < trials; ++trial)
                    run(counts, seed + trial, rule, frames, rate / 100.0f, blockage, params);
                std::cout << (blockage ? "blockage" : "interference") << ',' << rate << ',' << adrRuleName(rule)
                          << ',' << 100 * counts.above / observations << ',' << 100 * counts.lost / observations << ',';
                if (counts.reactions) std::cout << secondsPerFrame * counts.reactionFrames / counts.reactions;
                std::cout << ',' << counts.reactions << ',' << 3600 * counts.changes / (secondsPerFrame * observations)
                          << '\n';
            }
        }
    }
}
