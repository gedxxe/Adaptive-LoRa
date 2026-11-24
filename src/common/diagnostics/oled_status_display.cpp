#include "oled_status_display.h"

#if defined(ADAPTIVE_LORA_OLED)
#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace adaptive_lora {
namespace {
constexpr uint8_t address = 0x3C;
constexpr uint32_t busHz = 400000;
constexpr uint16_t timeoutMs = 3;
constexpr uint8_t pixelColumns = 128;
constexpr uint8_t pixelsPerChunk = 24;
constexpr uint64_t refreshUs = 1000000;
constexpr uint64_t pageUs = 5000000;
#if defined(ADAPTIVE_LORA_MULTICLIENT)
constexpr uint64_t faultRetryUs = 2000000;
#endif
constexpr auto vextPin = static_cast<gpio_num_t>(Vext);
constexpr auto resetPin = static_cast<gpio_num_t>(RST_OLED);
static_assert(SDA_OLED == 17 && SCL_OLED == 18 && RST_OLED == 21 && Vext == 36,
              "OLED build uses the Heltec V3 reference variant");

const char* linkName(LinkFreshness state) {
    switch (state) {
        case LinkFreshness::WAIT: return "WAIT";
        case LinkFreshness::ONLINE: return "ONLINE";
        case LinkFreshness::STALE: return "STALE";
    }
    return "WAIT";
}

void metric(char (&row)[kOledColumns + 1], const char* name, float value, bool valid, const char* unit) {
    if (valid && std::isfinite(value) && std::fabs(value) <= 999.9f)
        std::snprintf(row, sizeof(row), "%s %.1f %s", name, static_cast<double>(value), unit);
    else
        std::snprintf(row, sizeof(row), "%s N/A", name);
}

void age(char (&row)[kOledColumns + 1], const LastReception& sample, uint64_t now) {
    if (sample.seen && now >= sample.atUs)
        std::snprintf(row, sizeof(row), "AGE %" PRIu64 " S", (now - sample.atUs) / 1000000);
    else
        std::snprintf(row, sizeof(row), "AGE N/A");
}

// Column bitmaps for the status screen's uppercase alphabet and digits.
constexpr uint8_t letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x41,0x41,0x7F,0x41,0x41},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x26,0x49,0x49,0x49,0x32}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x7F,0x20,0x18,0x20,0x7F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
};
constexpr uint8_t digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x62,0x51,0x49,0x49,0x46},
    {0x22,0x41,0x49,0x49,0x36}, {0x18,0x14,0x12,0x7F,0x10}, {0x2F,0x49,0x49,0x49,0x31},
    {0x3E,0x49,0x49,0x49,0x32}, {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x26,0x49,0x49,0x49,0x3E},
};
uint8_t pixel(char c, uint8_t column) {
    if (column == 5) return 0;
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'][column];
    if (c >= '0' && c <= '9') return digits[c - '0'][column];
    if (c == '-') return 0x08;
    if (c == '.') return column == 2 ? 0x60 : 0;
    if (c == '/') return uint8_t(0x40 >> column);
    return 0;
}

bool output(gpio_num_t pin, uint32_t level) {
    // Preload the output latch while the pad is still an input.
    return gpio_set_level(pin, level) == 0 && gpio_set_direction(pin, GPIO_MODE_OUTPUT) == 0;
}
}  // namespace

OledText formatOledStatus(const DisplaySnapshot& s, uint64_t nowUs, uint8_t v3Page) {
#if !defined(ADAPTIVE_LORA_MULTICLIENT)
    (void)v3Page;
#endif
    OledText screen;
    auto& r = screen.rows;
    const bool counts = (nowUs / pageUs) % 2 != 0;
    if (s.configError) {
        std::snprintf(r[0], sizeof(r[0]), "ADAPTIVE-LORA %s", counts ? "2/2" : "1/2");
        std::snprintf(r[1], sizeof(r[1]), "PROVISIONING");
        std::snprintf(r[2], sizeof(r[2]), "CONFIG INVALID");
        std::snprintf(r[3], sizeof(r[3]), "RADIO %s", s.radioState);
        std::snprintf(r[4], sizeof(r[4]), "USE SERIAL STATUS");
        std::snprintf(r[5], sizeof(r[5]), "LOG DROP %" PRIu32, s.logDropped);
        return screen;
    }
    const bool gateway = s.config.role == DeviceRole::GATEWAY;
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    if (s.protocol == multiclient::kWireVersion) {
        const auto& v3 = s.multiclient;
        const auto& peer = v3.peer;
        const unsigned page = v3Page % 3;
        const char* title = page == 0 ? "LINK" : page == 1 ? "DATA" : "ACK";
        if (multiclient::kAdaptive) {
            const char* direction = gateway ? "RX" : "TX";
            if (v3.lastDataSf)
                std::snprintf(r[0], sizeof(r[0]), "%u/3 %s SF%u CR4/%u", page + 1, direction,
                              unsigned(v3.lastDataSf), unsigned(phy::codingRateDenominator));
            else
                std::snprintf(r[0], sizeof(r[0]), "%u/3 %s -- CR4/%u", page + 1, direction,
                              unsigned(phy::codingRateDenominator));
        } else std::snprintf(r[0], sizeof(r[0]), "V%u %s %u/3", unsigned(s.protocol), title, page + 1);
        const char* suffix = multiclient::kAdaptive ? title : "";
        if (!gateway && multiclient::kAdaptive) {
            switch (v3.clientIssue) {
                case ClientScheduleIssue::REQUEST_TX: suffix = "REQ FAIL"; break;
                case ClientScheduleIssue::NO_GRANT: suffix = "NO GRANT"; break;
                case ClientScheduleIssue::NO_DATA_TX: suffix = "NO TX"; break;
                case ClientScheduleIssue::GROUP_WAIT: suffix = "GROUP WAIT"; break;
                case ClientScheduleIssue::NONE: break;
            }
        }
        if (s.searchingForBeacon) suffix = "SEARCH";
        std::snprintf(r[1], sizeof(r[1]), "%s %u%s%s", gateway ? "GATEWAY" : "CLIENT",
                      unsigned(s.config.nodeId), *suffix ? " " : "", suffix);
        if (page == 0) {
            if (!gateway && !s.searchingForBeacon && v3.clientIssue == ClientScheduleIssue::GROUP_WAIT)
                std::snprintf(r[2], sizeof(r[2]), "BEACON SYNC");
            else if (peer.seen)
                std::snprintf(r[2], sizeof(r[2]), "LAST %s P %u", linkName(v3.freshness(nowUs)), unsigned(peer.peer));
            else
                std::snprintf(r[2], sizeof(r[2]), "LAST WAIT");
            metric(r[3], "RSSI", peer.rssi, peer.metrics, "DBM");
            metric(r[4], "SNR", peer.snr, peer.metrics, "DB");
            age(r[5], peer, nowUs);
            if (v3.frame) std::snprintf(r[6], sizeof(r[6]), "FRAME %" PRIu32, v3.frame);
            else std::snprintf(r[6], sizeof(r[6]), "FRAME N/A");
            if (v3.periodUs) std::snprintf(r[7], sizeof(r[7]), "PERIOD %" PRIu32 " MS", v3.periodUs / 1000);
            else std::snprintf(r[7], sizeof(r[7]), "PERIOD N/A");
        } else if (page == 1) {
            std::snprintf(r[2], sizeof(r[2]), "%s %" PRIu32, gateway ? "DATA RX" : "DATA TX", v3.data);
            std::snprintf(r[3], sizeof(r[3]), "RETRY %" PRIu32, v3.retries);
            std::snprintf(r[4], sizeof(r[4]), "APP OK %" PRIu32, v3.accepted);
            if (gateway) {
                std::snprintf(r[5], sizeof(r[5]), "REFUSED %" PRIu32, v3.refused);
                std::snprintf(r[6], sizeof(r[6]), "DUP %" PRIu32, v3.duplicates);
            } else {
                const char* result = "N/A";
                switch (v3.result) {
                    case PayloadResult::NONE: break;
                    case PayloadResult::ACCEPTED: result = "ACCEPTED"; break;
                    case PayloadResult::MISSING: result = "MISSING"; break;
                    case PayloadResult::UNKNOWN: result = "UNKNOWN"; break;
                    case PayloadResult::NOT_ADMITTED: result = "NOT ADMITTED"; break;
                }
                std::snprintf(r[5], sizeof(r[5]), "APP %s", result);
                if (multiclient::kAdaptive) {
                    if (v3.confirmedDataSf)
                        std::snprintf(r[6], sizeof(r[6]), "ACKED DATA SF%u", unsigned(v3.confirmedDataSf));
                    else std::snprintf(r[6], sizeof(r[6]), "ACKED DATA SF N/A");
                }
            }
            std::snprintf(r[7], sizeof(r[7]), "RADIO ERR %" PRIu32, v3.radioErrors);
        } else {
            if (v3.bitmap.seen) {
                std::snprintf(r[2], sizeof(r[2]), "ACK FRAME %" PRIu32, v3.bitmap.frame);
                std::snprintf(r[3], sizeof(r[3]), "ROUND %u", unsigned(v3.bitmap.round));
                std::snprintf(r[4], sizeof(r[4]), "ACCEPT MASK %02X", unsigned(v3.bitmap.accepted));
                std::snprintf(r[5], sizeof(r[5]), "RETRY MASK %02X", unsigned(v3.bitmap.retries));
            } else std::snprintf(r[2], sizeof(r[2]), "BITMAP N/A");
            std::snprintf(r[6], sizeof(r[6]), "RADIO %s %s", s.radioReady ? "OK" : "ERR", s.radioState);
            std::snprintf(r[7], sizeof(r[7]), "LOG DROP %" PRIu32, s.logDropped);
        }
        return screen;
    }
#endif
    std::snprintf(r[0], sizeof(r[0]), "ADAPTIVE-LORA %s", counts ? "2/2" : "1/2");
    const auto& last = gateway ? s.gateway.hello : s.client.ack;
    const char* link = linkName(last.freshness(nowUs, s.config.helloIntervalMs));
    if (gateway)
        std::snprintf(r[1], sizeof(r[1]), "GATEWAY %u", unsigned(s.config.nodeId));
    else
        std::snprintf(r[1], sizeof(r[1]), "CLIENT %u GW %u", unsigned(s.config.nodeId), unsigned(s.config.gatewayId));
    std::snprintf(r[2], sizeof(r[2]), "RADIO %s %s", s.radioReady ? "OK" : "ERR", s.radioState);
    if (counts && gateway) {
        std::snprintf(r[3], sizeof(r[3]), "HELLO %" PRIu32, s.gateway.uniqueHellos);
        std::snprintf(r[4], sizeof(r[4]), "DUP %" PRIu32, s.gateway.duplicates);
        std::snprintf(r[5], sizeof(r[5]), "ACK START %" PRIu32, s.gateway.ackStarts);
        std::snprintf(r[6], sizeof(r[6]), "ACK DONE %" PRIu32, s.gateway.ackCompletions);
        std::snprintf(r[7], sizeof(r[7]), "RADIO ERR %" PRIu32, s.gateway.radioErrors);
    } else if (counts) {
        std::snprintf(r[3], sizeof(r[3]), "HELLO %" PRIu32, s.client.attempts);
        std::snprintf(r[4], sizeof(r[4]), "ACK %" PRIu32, s.client.successes);
        std::snprintf(r[5], sizeof(r[5]), "TIMEOUT %" PRIu32, s.client.timeouts);
        std::snprintf(r[6], sizeof(r[6]), "TX FAIL %" PRIu32, s.client.txFailures);
        std::snprintf(r[7], sizeof(r[7]), "LOG DROP %" PRIu32, s.logDropped);
    } else {
        if (last.seen)
            std::snprintf(r[3], sizeof(r[3]), "%s PEER %u", link, unsigned(last.peer));
        else
            std::snprintf(r[3], sizeof(r[3]), "%s PEER N/A", link);
        metric(r[4], "RSSI", last.rssi, last.metrics, "DBM");
        metric(r[5], "SNR", last.snr, last.metrics, "DB");
        age(r[6], last, nowUs);
        if (gateway ? s.gateway.sessionUnavailable : s.client.sessionUnavailable)
            std::snprintf(r[7], sizeof(r[7]), "SESSION ERROR");
        else
            std::snprintf(r[7], sizeof(r[7]), "LOG DROP %" PRIu32, s.logDropped);
    }
    return screen;
}

void OledStatusDisplay::start(uint64_t nowUs) {
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    error_ = nullptr;
    lit_ = false;
    rotating_ = false;
    row_ = kOledRows;
    if (busReady_) {
        // Reset only the display. Leave the shared Vext rail and Wire alone.
        if (gpio_set_level(resetPin, 0) != 0) { fail("gpio_reset"); return; }
        deadlineUs_ = nowUs + 10000;
        stage_ = Stage::POWER_SETTLE;
        return;
    }
#endif
    if (!output(vextPin, 1) || !output(resetPin, 0) || gpio_set_level(vextPin, 0) != 0) {
        fail("gpio_setup");
        return;
    }
    deadlineUs_ = nowUs + 100000;
    stage_ = Stage::POWER_SETTLE;
}

void OledStatusDisplay::fail(const char* reason) {
    error_ = reason;
    stage_ = Stage::FAILED;
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    lit_ = false;
    deadlineUs_ = static_cast<uint64_t>(esp_timer_get_time()) + faultRetryUs;
#endif
}

bool OledStatusDisplay::needsSnapshot(uint64_t nowUs) const {
    if (stage_ != Stage::DRAW || row_ != kOledRows) return false;
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    if (rotating_ && nowUs >= nextPageAt_) return true;
#endif
    return nowUs >= deadlineUs_;
}

void OledStatusDisplay::render(const DisplaySnapshot& snapshot, uint64_t nowUs) {
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    v3Screen_ = !snapshot.configError && snapshot.protocol == multiclient::kWireVersion;
    rotateAfterDraw_ = v3Screen_;
    nextPage_ = rotateAfterDraw_ && rotating_
        ? (nowUs >= nextPageAt_ ? (visiblePage_ + 1) % 3 : visiblePage_) : 0;
    desired_ = formatOledStatus(snapshot, nowUs, nextPage_);
#else
    desired_ = formatOledStatus(snapshot, nowUs);
#endif
    deadlineUs_ = nowUs + refreshUs;
    row_ = 0;
    col_ = 0;
    refreshIoUs_ = 0;
}

bool OledStatusDisplay::write(const uint8_t* bytes, size_t size) {
    const auto started = esp_timer_get_time();
    Wire.beginTransmission(address);
    const size_t written = Wire.write(bytes, size);
    const uint8_t status = Wire.endTransmission();
    const auto elapsed = static_cast<uint32_t>(esp_timer_get_time() - started);
    if (elapsed > worstIoUs_) worstIoUs_ = elapsed;
    refreshIoUs_ += elapsed;
    if (written != size || status != 0) {
        fail(written != size ? "i2c_short_write" : "i2c_transfer");
        return false;
    }
    return true;
}

void OledStatusDisplay::finishDraw() {
    if (!lit_) {
        stage_ = Stage::LIGHT;
        return;
    }
    if (refreshIoUs_ > worstRefreshUs_) worstRefreshUs_ = refreshIoUs_;
#if defined(ADAPTIVE_LORA_MULTICLIENT)
    if (v3Screen_) {
        const auto now = static_cast<uint64_t>(esp_timer_get_time());
        if (rotateAfterDraw_ && (!rotating_ || nextPage_ != visiblePage_))
            nextPageAt_ = now + pageUs;
        deadlineUs_ = now + refreshUs;
    }
    rotating_ = rotateAfterDraw_;
    visiblePage_ = nextPage_;
#endif
}

void OledStatusDisplay::drawChunk() {
    const auto physicalRow = [this] {
#if defined(ADAPTIVE_LORA_MULTICLIENT)
        // During a split repaint, keep the old title until the new body is drawn.
        if (v3Screen_) return uint8_t((row_ + 1) % kOledRows);
#endif
        return row_;
    };
    // Unchanged text pages need no bus transfer. The first frame writes all
    // pages while the panel is off, to clear undefined display RAM.
    while (row_ < kOledRows && col_ == 0 && lit_ &&
           std::strcmp(desired_.rows[physicalRow()], shown_.rows[physicalRow()]) == 0)
        ++row_;
    if (row_ == kOledRows) {
        finishDraw();
        return;
    }

    const auto row = physicalRow();
    // Continuation-tagged commands select page and column, then one data block.
    uint8_t chunk[7 + pixelsPerChunk]{0x80, uint8_t(0xB0 | row),
        0x80, uint8_t(col_ & 0x0F), 0x80, uint8_t(0x10 | (col_ >> 4)), 0x40};
    const unsigned remaining = pixelColumns - col_;
    const unsigned count = remaining < pixelsPerChunk ? remaining : pixelsPerChunk;
    const auto textLength = std::strlen(desired_.rows[row]);
    for (unsigned i = 0; i < count; ++i) {
        const unsigned x = col_ + i;
        if (x / 6 < textLength) chunk[7 + i] = pixel(desired_.rows[row][x / 6], x % 6);
    }
    if (!write(chunk, 7 + count)) return;
    col_ += count;
    if (col_ == pixelColumns) {
        std::memcpy(shown_.rows[row], desired_.rows[row], sizeof(shown_.rows[row]));
        col_ = 0;
        ++row_;
        if (row_ == kOledRows) finishDraw();
    }
}

void OledStatusDisplay::service(uint64_t nowUs) {
    switch (stage_) {
        case Stage::POWER_SETTLE:
            if (nowUs < deadlineUs_) return;
            if (gpio_set_level(resetPin, 1) != 0) { fail("gpio_reset"); return; }
            deadlineUs_ = nowUs + 10000;
            stage_ = Stage::RESET_SETTLE;
            return;
        case Stage::RESET_SETTLE:
            if (nowUs < deadlineUs_) return;
#if defined(ADAPTIVE_LORA_MULTICLIENT)
            stage_ = busReady_ ? Stage::INIT : Stage::BUS_BEGIN;
#else
            stage_ = Stage::BUS_BEGIN;
#endif
            return;
        case Stage::BUS_BEGIN:
            if (!Wire.begin(SDA_OLED, SCL_OLED, busHz)) { fail("i2c_begin"); return; }
            Wire.setTimeOut(timeoutMs);
#if defined(ADAPTIVE_LORA_MULTICLIENT)
            busReady_ = true;
#endif
            stage_ = Stage::INIT;
            return;
        case Stage::INIT: {
            const uint8_t init[] = {0, 0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0, 0x40,
                0x8D, 0x14, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12, 0x81, 0x7F,
                0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0x2E};
            if (write(init, sizeof(init))) {
                stage_ = Stage::DRAW;
                deadlineUs_ = nowUs;
            }
            return;
        }
        case Stage::DRAW:
            if (row_ < kOledRows) drawChunk();
            return;
        case Stage::LIGHT: {
            const uint8_t on[] = {0, 0xAF};
            if (write(on, sizeof(on))) {
                lit_ = true;
                stage_ = Stage::DRAW;
                finishDraw();
            }
            return;
        }
        case Stage::FAILED:
#if defined(ADAPTIVE_LORA_MULTICLIENT)
            if (nowUs >= deadlineUs_) start(nowUs);
#endif
            return;
        case Stage::OFF:
            return;
    }
}
}  // namespace adaptive_lora
#endif
