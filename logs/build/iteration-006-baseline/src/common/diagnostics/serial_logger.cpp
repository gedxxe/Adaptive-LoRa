#include "serial_logger.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <esp_timer.h>
#include "radio_config.h"

namespace adaptive_lora {
void SerialLogger::text(const char* line) {
    if (count_ == 16) { ++dropped_; return; }
    const int size = std::snprintf(lines_[tail_], sizeof(lines_[tail_]), "%s\n", line);
    if (size < 0 || static_cast<size_t>(size) >= sizeof(lines_[tail_])) { ++dropped_; return; }
    tail_ = (tail_ + 1) % 16;
    ++count_;
}
void SerialLogger::event(const char* name, const char* result, const Frame* frame,
                         const RadioEvent* rx, int16_t code, uint32_t toa) {
    char line[512];
    char identity[192];
    if (frame) std::snprintf(identity, sizeof(identity),
        "source_id=%u,destination_id=%u,sequence=%u,boot_id=%" PRIu32 ",frame_id=%" PRIu32 ",config_version=%u,schedule_version=%u",
        frame->sourceId, frame->destinationId, frame->sequence, frame->bootId, frame->frameId, frame->configVersion, frame->scheduleVersion);
    else std::snprintf(identity, sizeof(identity), "source_id=N/A,destination_id=N/A,sequence=N/A,boot_id=N/A,frame_id=N/A,config_version=N/A,schedule_version=N/A");
    char metrics[64];
    if (rx && rx->metrics) std::snprintf(metrics, sizeof(metrics), "rssi_dbm=%.1f,snr_db=%.1f", rx->rssi, rx->snr);
    else std::snprintf(metrics, sizeof(metrics), "rssi_dbm=N/A,snr_db=N/A");
    char airtime[24];
    if (toa) std::snprintf(airtime, sizeof(airtime), "%" PRIu32, toa);
    else std::snprintf(airtime, sizeof(airtime), "N/A");
    const int size = std::snprintf(line, sizeof(line),
        "uptime_us=%" PRId64 ",role=%s,network_id=%u,node_id=%u,event=%s,%s,sf=%u,bw_khz=%.0f,freq_mhz=%.3f,%s,toa_us=%s,code=%d,result=%s",
        esp_timer_get_time(), roleName(config_.role), config_.networkId, config_.nodeId, name, identity,
        phy::dataSf, phy::bandwidthKhz, phy::frequencyMhz, metrics, airtime, code, result);
    if (size < 0 || static_cast<size_t>(size) >= sizeof(line)) { ++dropped_; return; }
    text(line);
}
void SerialLogger::drain() {
    if (!count_) return;
    const int available = Serial.availableForWrite();
    if (available <= 0) return;
    size_t bytes = std::strlen(lines_[head_]) - offset_;
    if (bytes > static_cast<size_t>(available)) bytes = static_cast<size_t>(available);
    if (bytes > 128) bytes = 128;
    const size_t written = Serial.write(reinterpret_cast<const uint8_t*>(lines_[head_] + offset_), bytes);
    offset_ += written;
    if (lines_[head_][offset_] == '\0') {
        offset_ = 0;
        head_ = (head_ + 1) % 16;
        --count_;
    }
}
}  // namespace adaptive_lora
