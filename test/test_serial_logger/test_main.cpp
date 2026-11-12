#include <unity.h>
#include <Arduino.h>
#include <esp_timer.h>
#include <string>
#include <cstring>
#include <cstdio>
#include "common/diagnostics/serial_logger.h"
using namespace adaptive_lora;
namespace {
void finish(SerialLogger& log) {
    for (unsigned n = 0; !log.empty() && n < 512; ++n) log.drain();
    TEST_ASSERT_TRUE(log.empty());
}
void test_queue_bound_and_fifo() {
    SerialLogger log;
    std::string expected;
    for (unsigned i = 0; i < 16; ++i) {
        char line[24]; std::snprintf(line, sizeof(line), "line-%u", i);
        log.text(line); expected += std::string(line) + "\n";
    }
    TEST_ASSERT_EQUAL_UINT32(0, log.dropped());
    log.text("overflow"); TEST_ASSERT_EQUAL_UINT32(1, log.dropped());
    finish(log);
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), Serial.output.c_str());
    TEST_ASSERT_TRUE(Serial.output.find("overflow") == std::string::npos);
    unsigned lines = 0;
    for (char ch : Serial.output) if (ch == '\n') ++lines;
    TEST_ASSERT_EQUAL_UINT(16, lines);
    log.text("after"); finish(log);
    TEST_ASSERT_TRUE(Serial.output.find("after\n") != std::string::npos);
}
void test_backpressure_retains_queued_line_without_write() {
    SerialLogger log; log.text("blocked"); Serial.capacity = 0;
    for (unsigned n = 0; n < 100; ++n) log.drain();
    TEST_ASSERT_EQUAL_UINT(0, Serial.calls); TEST_ASSERT_FALSE(log.empty());
    Serial.capacity = 64; finish(log);
    TEST_ASSERT_EQUAL_STRING("blocked\n", Serial.output.c_str());
}
void test_short_writes_preserve_bytes_and_128_byte_budget() {
    SerialLogger log; char line[301]; std::memset(line, 'x', 300); line[300] = '\0';
    log.text(line); Serial.capacity = 1024; Serial.writeLimit = 3;
    finish(log);
    TEST_ASSERT_EQUAL_STRING((std::string(line) + "\n").c_str(), Serial.output.c_str());
    TEST_ASSERT_TRUE(Serial.largestWrite <= 128);
    TEST_ASSERT_TRUE(Serial.calls > 1);
}
void test_zero_byte_write_does_not_discard_line() {
    SerialLogger log; log.text("retry drain"); Serial.writeLimit = 0;
    log.drain(); TEST_ASSERT_FALSE(log.empty()); TEST_ASSERT_TRUE(Serial.output.empty());
    Serial.writeLimit = 128; finish(log);
    TEST_ASSERT_EQUAL_STRING("retry drain\n", Serial.output.c_str());
}
void test_oversized_text_counted_without_partial_line() {
    SerialLogger log; char huge[600]; std::memset(huge, 'x', sizeof(huge) - 1); huge[sizeof(huge) - 1] = '\0';
    log.text(huge); TEST_ASSERT_TRUE(log.empty()); TEST_ASSERT_EQUAL_UINT32(1, log.dropped());
    log.text("valid"); finish(log); TEST_ASSERT_EQUAL_STRING("valid\n", Serial.output.c_str());
}
void test_unavailable_metrics_are_na_and_real_formatter_is_used() {
    DeviceConfig c; c.role = DeviceRole::CLIENT; c.nodeId = 2;
    SerialLogger log; log.begin(c); host_clock::set(123456789);
    log.event("FRAME_REJECT", "test"); finish(log);
    TEST_ASSERT_TRUE(Serial.output.find("uptime_us=123456789,role=CLIENT,network_id=1,node_id=2,event=FRAME_REJECT,") == 0);
    TEST_ASSERT_TRUE(Serial.output.find("rssi_dbm=N/A,snr_db=N/A,toa_us=N/A") != std::string::npos);
    TEST_ASSERT_EQUAL_UINT32(0, log.dropped());
}
}
void setUp() { Serial.reset(); host_clock::set(0); }
void tearDown() {}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_queue_bound_and_fifo);
    RUN_TEST(test_backpressure_retains_queued_line_without_write);
    RUN_TEST(test_short_writes_preserve_bytes_and_128_byte_budget);
    RUN_TEST(test_zero_byte_write_does_not_discard_line);
    RUN_TEST(test_oversized_text_counted_without_partial_line);
    RUN_TEST(test_unavailable_metrics_are_na_and_real_formatter_is_used);
    return UNITY_END();
}
