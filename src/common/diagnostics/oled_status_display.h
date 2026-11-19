#pragma once
#include "display_snapshot.h"
#include <cstddef>

namespace adaptive_lora {
constexpr size_t kOledRows = 8;
constexpr size_t kOledColumns = 21;
struct OledText {
    char rows[kOledRows][kOledColumns + 1]{};
};

OledText formatOledStatus(const DisplaySnapshot& snapshot, uint64_t nowUs);

class OledStatusDisplay {
public:
    // Runtime calls service only after primary loop work and outside TX/ACK wait.
    void start(uint64_t nowUs);
    bool needsSnapshot(uint64_t nowUs) const;
    void render(const DisplaySnapshot& snapshot, uint64_t nowUs);
    void service(uint64_t nowUs);
    const char* error() const { return error_; }
    bool ready() const { return lit_; }
    uint32_t worstIoUs() const { return worstIoUs_; }
    uint32_t worstRefreshUs() const { return worstRefreshUs_; }

private:
    enum class Stage { OFF, POWER_SETTLE, RESET_SETTLE, BUS_BEGIN, INIT, DRAW, LIGHT, FAILED };
    void fail(const char* reason);
    bool write(const uint8_t* bytes, size_t size);
    void drawChunk();

    Stage stage_ = Stage::OFF;
    const char* error_ = nullptr;
    uint64_t deadlineUs_ = 0;
    OledText desired_;
    OledText shown_;
    uint8_t row_ = kOledRows;
    uint8_t col_ = 0;
    bool lit_ = false;
    uint32_t worstIoUs_ = 0;
    uint32_t refreshIoUs_ = 0;
    uint32_t worstRefreshUs_ = 0;
};
}  // namespace adaptive_lora
