#pragma once
#include <cstddef>
#include <cstdint>

namespace adaptive_lora {
// Return a nonzero ID different from previous, or zero on bounded failure.
using BootIdGenerator = uint32_t (*)(uint32_t previous);
struct PacketIdentity {
    uint16_t networkId;
    uint16_t sourceId;
    uint32_t bootId;
    uint16_t sequence;
};
enum class SequenceResult { NEW, DUPLICATE, STALE };
class SequenceTracker {
public:
    SequenceResult observe(const PacketIdentity& identity);
private:
    struct Session {
        bool used = false;
        uint16_t network = 0;
        uint16_t source = 0;
        uint32_t boot = 0;
        uint16_t highest = 0;
        uint64_t seen = 0;
    };
    Session sessions_[8]{};
    size_t replace_ = 0;
};
class SequenceCounter {
public:
    void begin(uint32_t boot) { boot_ = boot; next_ = 0; }
    bool take(uint16_t& sequence);
    bool renew(BootIdGenerator generate);
    uint32_t bootId() const { return boot_; }
private:
    uint32_t boot_ = 0;
    uint32_t next_ = 0;
};
}  // namespace adaptive_lora
