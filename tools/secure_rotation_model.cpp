// SPDX-License-Identifier: GPL-3.0-only
// Analytical channel experiment. The selector, airtime, retry masks and ADR
// rules are production code. Reception draws are a model, not radio timings.
#include "app/adr_policy.h"
#include "common/protocol/multiclient_protocol.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

namespace {
using namespace adaptive_lora;
using namespace adaptive_lora::multiclient;
struct Profile { const char* name; bool adaptive; AdrRule rule; };
constexpr Profile policies[]{
    {"FIXED_SF7", false, AdrRule::LEGACY}, {"DELIVERY_WINDOW", true, AdrRule::LEGACY},
    {"EWMA", true, AdrRule::MARGIN_EWMA}, {"KALMAN", true, AdrRule::MARGIN_KALMAN},
};
constexpr float decodeThreshold[]{-7.5f, -10.0f, -12.5f, -15.0f};
static_assert(sizeof(decodeThreshold) / sizeof(float) == phy::maxDataSf - phy::dataSf + 1);

struct Node {
    AdrState adr;
    uint32_t opportunity = 0;
    uint32_t receipt = 0;
    uint32_t delivered = 0;
    uint32_t served = 0;
};
struct Channel {
    float snr = 0;
    float requestSnr = 0;
    float beacon = 0, request = 0, grant = 0;
    float data[kAttempts]{}, feedback[kAttempts]{};
};
struct Report {
    unsigned offers = 0, requests = 0, first = 0, delivered = 0, attempts = 0;
    unsigned noDelivery = 0;
    uint64_t gatewayUs = 0, dataUs = 0, worstTail = 0;
    uint64_t maximumServiceGap = 0;
    std::vector<uint64_t> deliveryGaps;
};

bool heard(float draw, float snr, uint8_t sf, bool burst) {
    const float overlap = burst ? 0.65f : 0.03f;
    const float curve = 1.0f / (1.0f + std::exp((decodeThreshold[sf - kBaseSf] - snr) / 0.75f));
    return draw < (1.0f - overlap) * curve;
}

void run(uint16_t population, uint32_t seed, uint32_t frames, Profile profile) {
    std::array<Node, kPopulationLimit> nodes;
    std::array<Channel, kPopulationLimit> channel;
    std::array<uint8_t, kPopulationLimit> sf;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> draw(0, 1);
    std::normal_distribution<float> noise(0, 1);
    Report result;
    Rotation rotation{population};
    bool burst = false;
    constexpr uint8_t bytes = 32;
    constexpr uint64_t frameSeconds = kAdaptivePeriodUs / 1000000;

    for (uint32_t frame = 1; frame <= frames; ++frame) {
        burst = draw(rng) < (burst ? 0.82f : 0.035f);
        for (uint16_t i = 0; i < population; ++i) {
            auto& link = channel[i];
            const bool blocked = i % 4 == 0 && (frame + 17 * i) % 180 < 60;
            link.snr = -2.0f - 1.4f * (i % 7) - (blocked ? 7.0f : 0.0f);
            link.requestSnr = link.snr + noise(rng);
            link.beacon = draw(rng);
            link.request = draw(rng);
            link.grant = draw(rng);
            for (uint8_t round = 0; round < kAttempts; ++round) {
                link.data[round] = draw(rng);
                link.feedback[round] = draw(rng);
            }
            const auto& state = nodes[i].adr;
            sf[i] = profile.adaptive ? std::max(state.desired, state.reservedSf) : kBaseSf;
        }
        const auto roster = rotation.select(1, bytes, sf.data());
        result.gatewayUs += beaconAir(roster.count);
        uint8_t requested = 0, granted = 0, delivered = 0, first = 0, eligible = 0;
        uint8_t selectedSf[kMaxClients]{};
        for (uint8_t slot = 0; slot < roster.count; ++slot) {
            const auto i = registryPosition(roster.nodes[slot].id, 1);
            auto& peer = nodes[i];
            const auto& link = channel[i];
            ++result.offers;
            ++peer.opportunity;
            if (peer.served)
                result.maximumServiceGap = std::max(result.maximumServiceGap, (frame - peer.served) * frameSeconds);
            peer.served = frame;
            if (profile.adaptive) expireAdrProbe(peer.adr, peer.opportunity);
            if (!heard(link.beacon, link.snr, kControlSf, burst) || !heard(link.request, link.snr, kControlSf, burst))
                continue;
            ++result.requests;
            requested |= slotBit(slot);
            if (profile.adaptive) {
                applyAdrReceipt(peer.adr, peer.receipt);
                if (peer.adr.lastRequest && peer.opportunity - peer.adr.lastRequest >= kAdrParams.idleFrames)
                    resumeAdrAfterIdle(peer.adr, peer.opportunity);
                peer.adr.lastRequest = peer.opportunity;
                peer.adr.reservedSf = std::max(peer.adr.reservedSf, std::min(sf[i], peer.adr.desired));
            }
            selectedSf[slot] = std::min(sf[i], peer.adr.desired);
            if (heard(link.grant, link.snr, kControlSf, burst)) granted |= slotBit(slot);
        }
        if (requested) result.gatewayUs += grantAir(roster.count) + kAttempts * feedbackAir(roster.count);
        eligible = granted;
        for (uint8_t round = 0; requested && round < kAttempts; ++round) {
            for (uint8_t slot = 0; slot < roster.count; ++slot) {
                const auto bit = slotBit(slot);
                if (!(eligible & bit)) continue;
                const auto i = registryPosition(roster.nodes[slot].id, 1);
                ++result.attempts;
                result.dataUs += dataAir(bytes, selectedSf[slot]);
                if (heard(channel[i].data[round], channel[i].snr, selectedSf[slot], burst)) {
                    delivered |= bit;
                    if (!round) first |= bit;
                }
            }
            eligible = retryMask(requested & ~delivered, roster.count,
                                 nodes[registryPosition(roster.nodes[0].id, 1)].opportunity, round) & granted;
            for (uint8_t slot = 0; slot < roster.count; ++slot) {
                const auto i = registryPosition(roster.nodes[slot].id, 1);
                if (!heard(channel[i].feedback[round], channel[i].snr, kControlSf, burst)) eligible &= ~slotBit(slot);
                else if (delivered & slotBit(slot)) nodes[i].receipt = frame;
            }
        }
        for (uint8_t slot = 0; slot < roster.count; ++slot) {
            const auto bit = slotBit(slot);
            if (!(requested & bit)) continue;
            const auto i = registryPosition(roster.nodes[slot].id, 1);
            auto& peer = nodes[i];
            const bool accepted = delivered & bit;
            if (profile.adaptive)
                reviewAdr(peer.adr, profile.rule, selectedSf[slot], peer.opportunity,
                          {classifyAdr(true, true, accepted, (first & bit) == 0), channel[i].requestSnr});
            if (!accepted) continue;
            if (peer.delivered) result.deliveryGaps.push_back((frame - peer.delivered) * frameSeconds);
            peer.delivered = frame;
            peer.adr.deliveredFrame = frame;
            peer.adr.deliveredOpportunity = peer.opportunity;
            peer.adr.deliveredSf = selectedSf[slot];
            ++result.delivered;
            result.first += bool(first & bit);
        }
    }
    for (uint16_t i = 0; i < population; ++i) {
        result.noDelivery += !nodes[i].delivered;
        result.worstTail = std::max(result.worstTail, (frames - nodes[i].delivered) * frameSeconds);
    }
    std::sort(result.deliveryGaps.begin(), result.deliveryGaps.end());
    auto percentile = [&](size_t pct) {
        const auto n = result.deliveryGaps.size();
        return n ? result.deliveryGaps[(n * pct + 99) / 100 - 1] : 0;
    };
    std::cout << population << ',' << profile.name << ',' << result.offers << ',' << result.requests << ','
              << result.first << ',' << result.delivered << ',' << result.attempts << ',' << result.noDelivery << ','
              << result.maximumServiceGap << ',' << percentile(50) << ',' << percentile(95) << ','
              << percentile(100) << ',' << result.worstTail << ',' << result.gatewayUs << ',' << result.dataUs << '\n';
}
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const auto seed = uint32_t(std::strtoul(argv[1], nullptr, 10));
    const auto frames = uint32_t(std::strtoul(argv[2], nullptr, 10));
    if (!frames || frames > 57600) return 2;
    std::cout << "MODEL-ONLY,seed=" << seed << ",frames=" << frames << '\n';
    std::cout << "population,rule,offered,requested,first,delivered,data_attempts,never_delivered,"
                 "max_service_gap_s,p50_delivery_gap_s,p95_delivery_gap_s,max_delivery_gap_s,max_censored_tail_s,"
                 "gateway_tx_us,client_data_tx_us\n";
    for (auto population : {8, 70, 84, 128})
        for (const auto& policy : policies) run(uint16_t(population), seed, frames, policy);
}
