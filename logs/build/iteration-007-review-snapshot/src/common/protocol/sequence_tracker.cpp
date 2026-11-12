#include "sequence_tracker.h"

namespace adaptive_lora {
SequenceResult SequenceTracker::observe(const PacketIdentity& id) {
    for (auto& s : sessions_) {
        if (!s.used || s.network != id.networkId || s.source != id.sourceId || s.boot != id.bootId) continue;
        if (id.sequence > s.highest) {
            const unsigned delta = id.sequence - s.highest;
            s.seen = delta >= 64 ? 1 : (s.seen << delta) | 1;
            s.highest = id.sequence;
            return SequenceResult::NEW;
        }
        const unsigned age = s.highest - id.sequence;
        if (age >= 64) return SequenceResult::STALE;
        const uint64_t mask = uint64_t(1) << age;
        if (s.seen & mask) return SequenceResult::DUPLICATE;
        s.seen |= mask;
        return SequenceResult::NEW;
    }
    Session& s = sessions_[replace_];
    replace_ = (replace_ + 1) % 8;
    s = {true, id.networkId, id.sourceId, id.bootId, id.sequence, 1};
    return SequenceResult::NEW;
}
bool SequenceCounter::take(uint16_t& sequence) {
    if (boot_ == 0 || next_ > 65535) return false;
    sequence = static_cast<uint16_t>(next_++);
    return true;
}
}  // namespace adaptive_lora
