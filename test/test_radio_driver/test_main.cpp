#include <unity.h>
#include <cstring>
#include <initializer_list>
#include <limits>
#include "common/radio/radio_driver.h"
#include "../support/fake_radio_hardware.h"

using namespace adaptive_lora;
using namespace adaptive_lora::test;
namespace {
constexpr int16_t injected = -701;  // Synthetic, opaque RadioLib-operation failure.
constexpr int16_t cleanupError = -702;
constexpr uint32_t unrelated = 0x0080;  // CAD_DONE, not a TX/RX completion.

void state(const RadioDriver& d, bool initialized, bool healthy, bool rx = false, bool tx = false) {
    TEST_ASSERT_EQUAL(initialized, d.initialized());
    TEST_ASSERT_EQUAL(healthy, d.healthy());
    TEST_ASSERT_EQUAL(rx, d.receiving());
    TEST_ASSERT_EQUAL(tx, d.transmitting());
}
void eventIs(const RadioEvent& e, RadioEventType type, int16_t code, bool metrics = false) {
    TEST_ASSERT_EQUAL_INT(int(type), int(e.type));
    TEST_ASSERT_EQUAL_INT16(code, e.code);
    TEST_ASSERT_EQUAL(metrics, e.metrics);
}
void order(const FakeRadioHardware& h, std::initializer_list<HardwareCall> expected) {
    TEST_ASSERT_FALSE(h.violation);
    TEST_ASSERT_EQUAL_UINT(expected.size(), h.traceLength);
    size_t i = 0;
    for (auto call : expected) TEST_ASSERT_EQUAL_INT(int(call), int(h.trace[i++]));
}
struct Fixture {
    FakeRadioHardware h;
    RadioDriver d{h};
    uint8_t packet[kMaxFrameSize]{};
    Fixture() {
        for (size_t i = 0; i < sizeof(packet); ++i) packet[i] = static_cast<uint8_t>(i ^ 0xa5);
        std::memcpy(h.rxBytes, packet, sizeof(packet));
    }
    void init() { TEST_ASSERT_EQUAL_INT16(0, d.begin()); h.resetTrace(); }
    void rx() { init(); TEST_ASSERT_EQUAL_INT16(0, d.receive()); h.resetTrace(); }
    void tx(uint32_t now = 100) {
        init(); TEST_ASSERT_EQUAL_INT16(0, d.transmit(packet, kHeaderSize, now)); h.resetTrace();
    }
    RadioEvent receiveEvent(uint32_t flags = radio_irq::rxDone) { h.signal(flags); return d.poll(101); }
    void noMetrics() {
        TEST_ASSERT_EQUAL_UINT(0, h.count(HardwareCall::RSSI));
        TEST_ASSERT_EQUAL_UINT(0, h.count(HardwareCall::SNR));
    }
};

void test_init_success_and_order() {
    Fixture f;
    TEST_ASSERT_EQUAL_INT16(0, f.d.begin());
    state(f.d, true, true);
    TEST_ASSERT_EQUAL_UINT8(2, f.h.crcLength);
    order(f.h, {HardwareCall::BEGIN, HardwareCall::HEADER, HardwareCall::CRC, HardwareCall::ENABLE_IRQ});
    TEST_ASSERT_EQUAL_STRING("LLCC68_driver_id_not_exposed", f.d.chip());
}
void test_init_radio_failure_short_circuits() {
    Fixture f; f.h.beginCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.begin());
    state(f.d, false, false); order(f.h, {HardwareCall::BEGIN});
}
void test_init_header_failure_short_circuits() {
    Fixture f; f.h.headerCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.begin());
    state(f.d, false, false); order(f.h, {HardwareCall::BEGIN, HardwareCall::HEADER});
}
void test_init_crc_failure_does_not_enable_irq() {
    Fixture f; f.h.crcCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.begin());
    state(f.d, false, false); order(f.h, {HardwareCall::BEGIN, HardwareCall::HEADER, HardwareCall::CRC});
}
void test_failed_init_rejects_operations_and_poll() {
    Fixture f; f.h.beginCode = injected; TEST_ASSERT_EQUAL_INT16(injected, f.d.begin()); f.h.resetTrace();
    TEST_ASSERT_EQUAL_INT16(radio_status::unknown, f.d.receive());
    TEST_ASSERT_EQUAL_INT16(radio_status::packetTooLong, f.d.transmit(f.packet, kHeaderSize, 0));
    TEST_ASSERT_EQUAL_INT16(0, f.d.stop());
    f.h.signal(radio_irq::rxDone);
    eventIs(f.d.poll(0), RadioEventType::NONE, 0); state(f.d, false, false); order(f.h, {});
}
void test_init_retry_after_failure() {
    Fixture f; f.h.beginCode = injected; TEST_ASSERT_EQUAL_INT16(injected, f.d.begin());
    f.h.beginCode = 0; f.init(); state(f.d, true, true);
    TEST_ASSERT_EQUAL_INT16(0, f.d.receive()); state(f.d, true, true, true);
}
void test_repeated_begin_failure_retains_prior_initialized_flag() {
    Fixture f; f.init(); f.h.headerCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.begin()); state(f.d, true, false);
    TEST_ASSERT_EQUAL_INT16(0, f.d.stop()); state(f.d, true, true);
}
void test_uninitialized_poll_ignores_irq() {
    Fixture f; f.h.signal(radio_irq::txDone | radio_irq::rxDone);
    eventIs(f.d.poll(UINT32_MAX), RadioEventType::NONE, 0); state(f.d, false, false); order(f.h, {});
}
void test_receive_before_init() {
    Fixture f; TEST_ASSERT_EQUAL_INT16(radio_status::unknown, f.d.receive());
    state(f.d, false, false); order(f.h, {});
}
void test_receive_while_transmitting_preserves_tx() {
    Fixture f; f.tx(); TEST_ASSERT_EQUAL_INT16(radio_status::unknown, f.d.receive());
    state(f.d, true, true, false, true); order(f.h, {});
}
void test_receive_start_clears_latch_and_enters_rx() {
    Fixture f; f.init(); f.h.irqFlag = true;
    TEST_ASSERT_EQUAL_INT16(0, f.d.receive()); TEST_ASSERT_FALSE(f.h.irqFlag);
    state(f.d, true, true, true); order(f.h, {HardwareCall::CLEAR_SIGNAL, HardwareCall::START_RX});
}
void test_receive_start_failure_and_recovery() {
    Fixture f; f.init(); f.h.startRxCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.receive()); state(f.d, true, false);
    eventIs(f.d.poll(100), RadioEventType::NONE, 0);
    f.h.startRxCode = 0;
    TEST_ASSERT_EQUAL_INT16(0, f.d.receive()); state(f.d, true, true, true);
}
void test_receive_restart_while_receiving() {
    Fixture f; f.rx(); TEST_ASSERT_EQUAL_INT16(0, f.d.receive());
    state(f.d, true, true, true); order(f.h, {HardwareCall::CLEAR_SIGNAL, HardwareCall::START_RX});
}
void test_transmit_null_buffer() {
    Fixture f; f.init(); TEST_ASSERT_EQUAL_INT16(radio_status::packetTooLong, f.d.transmit(nullptr, kHeaderSize, 0));
    state(f.d, true, true); order(f.h, {});
}
void test_transmit_every_short_length() {
    Fixture f; f.init();
    for (size_t length = 0; length < kHeaderSize; ++length)
        TEST_ASSERT_EQUAL_INT16(radio_status::packetTooLong, f.d.transmit(f.packet, length, 0));
    state(f.d, true, true); order(f.h, {});
}
void test_transmit_oversize_lengths() {
    Fixture f; f.init();
    for (size_t length : {kMaxFrameSize + 1, size_t(256), std::numeric_limits<size_t>::max()})
        TEST_ASSERT_EQUAL_INT16(radio_status::packetTooLong, f.d.transmit(f.packet, length, 0));
    state(f.d, true, true); order(f.h, {});
}
void test_transmit_maximum_valid_packet_and_order() {
    Fixture f; f.init(); f.h.irqFlag = true;
    TEST_ASSERT_EQUAL_INT16(0, f.d.transmit(f.packet, kMaxFrameSize, 100));
    state(f.d, true, true, false, true); TEST_ASSERT_FALSE(f.h.irqFlag);
    TEST_ASSERT_EQUAL_UINT(kMaxFrameSize, f.h.txLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(f.packet, f.h.txBytes, kMaxFrameSize);
    order(f.h, {HardwareCall::STANDBY, HardwareCall::CLEAR_SIGNAL, HardwareCall::START_TX});
}
void test_transmit_before_init() {
    Fixture f; TEST_ASSERT_EQUAL_INT16(radio_status::packetTooLong, f.d.transmit(f.packet, kHeaderSize, 0));
    state(f.d, false, false); order(f.h, {});
}
void test_transmit_while_transmitting_preserves_packet_and_deadline() {
    Fixture f; f.tx();
    TEST_ASSERT_EQUAL_INT16(radio_status::packetTooLong, f.d.transmit(f.packet, kMaxFrameSize, 500));
    state(f.d, true, true, false, true); order(f.h, {});
    TEST_ASSERT_EQUAL_UINT(kHeaderSize, f.h.txLength);
    eventIs(f.d.poll(100 + kTxTimeoutUs), RadioEventType::ERROR, radio_status::txTimeout);
}
void test_transmit_standby_failure() {
    Fixture f; f.rx(); f.h.standbyCode = injected; f.h.irqFlag = true;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.transmit(f.packet, kHeaderSize, 100));
    state(f.d, true, false); TEST_ASSERT_TRUE(f.h.irqFlag); order(f.h, {HardwareCall::STANDBY});
}
void test_transmit_start_failure_then_receive_recovery() {
    Fixture f; f.init(); f.h.startTxCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.transmit(f.packet, kHeaderSize, 100)); state(f.d, true, false);
    order(f.h, {HardwareCall::STANDBY, HardwareCall::CLEAR_SIGNAL, HardwareCall::START_TX});
    TEST_ASSERT_EQUAL_INT16(0, f.d.receive()); state(f.d, true, true, true);
}
void test_transmit_from_rx_and_fault() {
    Fixture f; f.rx(); f.h.standbyCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.stop()); state(f.d, true, false);
    f.h.standbyCode = 0;
    TEST_ASSERT_EQUAL_INT16(0, f.d.transmit(f.packet, kHeaderSize, 100)); state(f.d, true, true, false, true);
    TEST_ASSERT_EQUAL_INT16(0, f.d.stop()); TEST_ASSERT_EQUAL_INT16(0, f.d.receive());
    TEST_ASSERT_EQUAL_INT16(0, f.d.transmit(f.packet, kHeaderSize, 200)); state(f.d, true, true, false, true);
}
void test_airtime_forwards_length_and_result() {
    Fixture f; f.init(); f.h.airtime = 43210;
    TEST_ASSERT_EQUAL_UINT32(43210, f.d.timeOnAir(kMaxFrameSize));
    TEST_ASSERT_EQUAL_UINT(kMaxFrameSize, f.h.airtimeLength); order(f.h, {HardwareCall::AIRTIME});
}
void test_idle_poll_does_not_touch_hardware() {
    Fixture f; f.init(); f.h.signal(radio_irq::rxDone);
    eventIs(f.d.poll(0), RadioEventType::NONE, 0); order(f.h, {});
}
void test_tx_poll_no_pending_irq() {
    Fixture f; f.tx(); eventIs(f.d.poll(101), RadioEventType::NONE, 0);
    state(f.d, true, true, false, true); order(f.h, {HardwareCall::SIGNAL, HardwareCall::CLEAR_SIGNAL});
}
void test_rx_poll_no_pending_irq() {
    Fixture f; f.rx(); eventIs(f.d.poll(101), RadioEventType::NONE, 0);
    state(f.d, true, true, true); order(f.h, {HardwareCall::SIGNAL, HardwareCall::CLEAR_SIGNAL});
}
void test_latched_irq_with_low_gpio_is_consumed() {
    Fixture f; f.tx(); f.h.signal(radio_irq::txDone, false, true);
    eventIs(f.d.poll(101), RadioEventType::TX_DONE, 0); TEST_ASSERT_FALSE(f.h.irqFlag); state(f.d, true, true);
}
void test_high_gpio_without_latched_irq_is_consumed() {
    Fixture f; f.tx(); f.h.signal(radio_irq::txDone, true, false);
    eventIs(f.d.poll(101), RadioEventType::TX_DONE, 0); state(f.d, true, true);
}
void test_tx_ignores_unrelated_and_zero_flags() {
    Fixture f; f.tx();
    for (uint32_t flags : {uint32_t(0), unrelated, radio_irq::rxDone, radio_irq::crcError}) {
        f.h.signal(flags); eventIs(f.d.poll(101), RadioEventType::NONE, 0); state(f.d, true, true, false, true);
    }
    TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::FINISH_TX));
}
void test_tx_done_and_completion_order() {
    Fixture f; f.tx(); f.h.signal(radio_irq::txDone);
    eventIs(f.d.poll(101), RadioEventType::TX_DONE, 0); state(f.d, true, true);
    order(f.h, {HardwareCall::SIGNAL, HardwareCall::CLEAR_SIGNAL, HardwareCall::FLAGS, HardwareCall::FINISH_TX});
    f.h.resetTrace(); eventIs(f.d.poll(102), RadioEventType::NONE, 0); order(f.h, {});
}
void test_tx_hardware_timeout_keeps_idle_after_successful_finish() {
    Fixture f; f.tx(); f.h.signal(radio_irq::timeout);
    eventIs(f.d.poll(101), RadioEventType::ERROR, radio_status::txTimeout); state(f.d, true, true);
}
void test_tx_done_and_timeout_prefers_timeout() {
    Fixture f; f.tx(); f.h.signal(radio_irq::txDone | radio_irq::timeout);
    eventIs(f.d.poll(101), RadioEventType::ERROR, radio_status::txTimeout); state(f.d, true, true);
}
void test_tx_finish_failure_and_stop_recovery() {
    Fixture f; f.tx(); f.h.finishTxCode = injected; f.h.signal(radio_irq::txDone);
    eventIs(f.d.poll(101), RadioEventType::ERROR, injected); state(f.d, true, false);
    TEST_ASSERT_EQUAL_INT16(0, f.d.stop()); state(f.d, true, true);
}
void test_tx_finish_failure_overrides_timeout_flag() {
    Fixture f; f.tx(); f.h.finishTxCode = injected; f.h.signal(radio_irq::timeout);
    eventIs(f.d.poll(101), RadioEventType::ERROR, injected); state(f.d, true, false);
}
void test_tx_deadline_immediately_before_expiration() {
    Fixture f; f.tx(); eventIs(f.d.poll(100 + kTxTimeoutUs - 1), RadioEventType::NONE, 0);
    state(f.d, true, true, false, true); TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::STANDBY));
}
void test_tx_deadline_at_expiration() {
    Fixture f; f.tx(); eventIs(f.d.poll(100 + kTxTimeoutUs), RadioEventType::ERROR, radio_status::txTimeout);
    state(f.d, true, true); order(f.h, {HardwareCall::STANDBY});
}
void test_tx_deadline_immediately_after_expiration() {
    Fixture f; f.tx(); eventIs(f.d.poll(100 + kTxTimeoutUs + 1), RadioEventType::ERROR, radio_status::txTimeout);
    state(f.d, true, true); order(f.h, {HardwareCall::STANDBY});
}
void test_tx_deadline_precedes_pending_tx_done() {
    Fixture f; f.tx(); f.h.signal(radio_irq::txDone);
    eventIs(f.d.poll(100 + kTxTimeoutUs), RadioEventType::ERROR, radio_status::txTimeout);
    order(f.h, {HardwareCall::STANDBY});
}
void test_tx_deadline_standby_failure_and_receive_recovery() {
    Fixture f; f.tx(); f.h.standbyCode = injected;
    eventIs(f.d.poll(100 + kTxTimeoutUs), RadioEventType::ERROR, injected); state(f.d, true, false);
    order(f.h, {HardwareCall::STANDBY}); f.h.standbyCode = 0;
    TEST_ASSERT_EQUAL_INT16(0, f.d.receive()); state(f.d, true, true, true);
}
void test_tx_deadline_across_uint32_wrap() {
    constexpr uint32_t start = UINT32_MAX - 1000;
    constexpr uint32_t deadline = start + kTxTimeoutUs;
    Fixture f; f.tx(start);
    eventIs(f.d.poll(start + 1), RadioEventType::NONE, 0);
    eventIs(f.d.poll(deadline - 1), RadioEventType::NONE, 0); state(f.d, true, true, false, true);
    eventIs(f.d.poll(deadline), RadioEventType::ERROR, radio_status::txTimeout); state(f.d, true, true);
}
void test_tx_deadline_after_uint32_wrap() {
    constexpr uint32_t start = UINT32_MAX - kTxTimeoutUs / 2;
    Fixture f; f.tx(start);
    eventIs(f.d.poll(start + kTxTimeoutUs + 1), RadioEventType::ERROR, radio_status::txTimeout);
    state(f.d, true, true);
}
void test_irrelevant_irq_does_not_extend_tx_deadline() {
    Fixture f; f.tx(); f.h.signal(unrelated);
    eventIs(f.d.poll(100 + kTxTimeoutUs - 1), RadioEventType::NONE, 0);
    f.h.resetTrace(); eventIs(f.d.poll(100 + kTxTimeoutUs), RadioEventType::ERROR, radio_status::txTimeout);
    order(f.h, {HardwareCall::STANDBY});
}
void test_rx_minimum_packet_and_call_order() {
    Fixture f; f.rx(); const auto e = f.receiveEvent();
    eventIs(e, RadioEventType::RX_DONE, 0, true); state(f.d, true, true);
    TEST_ASSERT_EQUAL_UINT(kHeaderSize, e.length); TEST_ASSERT_EQUAL_UINT(kHeaderSize, f.h.readLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(f.packet, e.bytes, e.length);
    TEST_ASSERT_EQUAL_FLOAT(f.h.rssi, e.rssi); TEST_ASSERT_EQUAL_FLOAT(f.h.snr, e.snr);
    order(f.h, {HardwareCall::SIGNAL, HardwareCall::CLEAR_SIGNAL, HardwareCall::FLAGS, HardwareCall::STANDBY,
        HardwareCall::LENGTH, HardwareCall::READ, HardwareCall::RSSI, HardwareCall::SNR, HardwareCall::FINISH_RX});
}
void test_rx_maximum_packet_no_buffer_overrun() {
    Fixture f; f.rx(); f.h.packetLength = kMaxFrameSize; const auto e = f.receiveEvent();
    eventIs(e, RadioEventType::RX_DONE, 0, true); TEST_ASSERT_EQUAL_UINT(kMaxFrameSize, e.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(f.packet, e.bytes, kMaxFrameSize); TEST_ASSERT_FALSE(f.h.violation);
}
void test_rx_timeout_no_payload_or_metrics() {
    Fixture f; f.rx(); eventIs(f.receiveEvent(radio_irq::timeout), RadioEventType::RX_TIMEOUT, radio_status::rxTimeout);
    state(f.d, true, true); f.noMetrics();
    TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::READ)); TEST_ASSERT_EQUAL_UINT(1, f.h.count(HardwareCall::FINISH_RX));
}
void test_rx_crc_error_without_rx_done() {
    Fixture f; f.rx(); eventIs(f.receiveEvent(radio_irq::crcError), RadioEventType::CRC_ERROR, radio_status::crcMismatch);
    state(f.d, true, true); f.noMetrics(); TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::READ));
}
void test_rx_header_error_without_valid_header() {
    Fixture f; f.rx(); eventIs(f.receiveEvent(radio_irq::headerError), RadioEventType::CRC_ERROR, radio_status::crcMismatch);
    state(f.d, true, true); f.noMetrics(); TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::READ));
}
void test_rx_header_error_with_valid_header_and_rx_done_is_accepted() {
    Fixture f; f.rx(); eventIs(f.receiveEvent(radio_irq::rxDone | radio_irq::headerError | radio_irq::headerValid),
        RadioEventType::RX_DONE, 0, true); state(f.d, true, true);
}
void test_rx_header_error_with_valid_header_alone_waits() {
    Fixture f; f.rx(); eventIs(f.receiveEvent(radio_irq::headerError | radio_irq::headerValid), RadioEventType::NONE, 0);
    state(f.d, true, true, true); TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::STANDBY));
}
void test_rx_crc_overrides_valid_header_and_rx_done() {
    Fixture f; f.rx(); eventIs(f.receiveEvent(radio_irq::rxDone | radio_irq::headerValid | radio_irq::crcError),
        RadioEventType::CRC_ERROR, radio_status::crcMismatch); f.noMetrics();
}
void test_rx_timeout_precedes_crc_and_data() {
    Fixture f; f.rx(); eventIs(f.receiveEvent(radio_irq::timeout | radio_irq::rxDone | radio_irq::crcError),
        RadioEventType::RX_TIMEOUT, radio_status::rxTimeout); f.noMetrics();
    TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::READ));
}
void test_rx_ignores_nonterminal_flags() {
    Fixture f; f.rx();
    for (uint32_t flags : {uint32_t(0), unrelated, radio_irq::txDone, radio_irq::headerValid}) {
        eventIs(f.receiveEvent(flags), RadioEventType::NONE, 0); state(f.d, true, true, true);
    }
    TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::STANDBY)); f.noMetrics();
}
void test_rx_every_short_packet_length() {
    for (size_t length = 0; length < kHeaderSize; ++length) {
        Fixture f; f.rx(); f.h.packetLength = length; const auto e = f.receiveEvent();
        eventIs(e, RadioEventType::ERROR, radio_status::packetTooLong); TEST_ASSERT_EQUAL_UINT(length, e.length);
        TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::READ)); TEST_ASSERT_EQUAL_UINT(1, f.h.count(HardwareCall::FINISH_RX));
        state(f.d, true, true); f.noMetrics();
    }
}
void test_rx_oversize_lengths_never_read_buffer() {
    for (size_t length : {kMaxFrameSize + 1, size_t(256), std::numeric_limits<size_t>::max()}) {
        Fixture f; f.rx(); f.h.packetLength = length; const auto e = f.receiveEvent();
        eventIs(e, RadioEventType::ERROR, radio_status::packetTooLong); TEST_ASSERT_EQUAL_UINT(length, e.length);
        TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::READ)); TEST_ASSERT_FALSE(f.h.violation); f.noMetrics();
    }
}
void test_rx_read_failure_propagates_without_metrics() {
    Fixture f; f.rx(); f.h.readCode = injected;
    eventIs(f.receiveEvent(), RadioEventType::ERROR, injected); state(f.d, true, true); f.noMetrics();
    TEST_ASSERT_EQUAL_UINT(1, f.h.count(HardwareCall::FINISH_RX));
}
void test_rx_read_crc_failure_is_classified() {
    Fixture f; f.rx(); f.h.readCode = radio_status::crcMismatch;
    eventIs(f.receiveEvent(), RadioEventType::CRC_ERROR, radio_status::crcMismatch); state(f.d, true, true); f.noMetrics();
}
void test_rx_read_timeout_retains_generic_error_classification() {
    Fixture f; f.rx(); f.h.readCode = radio_status::rxTimeout;
    eventIs(f.receiveEvent(), RadioEventType::ERROR, radio_status::rxTimeout); state(f.d, true, true); f.noMetrics();
}
void test_rx_standby_failure_skips_payload_and_finish() {
    Fixture f; f.rx(); f.h.standbyCode = injected;
    eventIs(f.receiveEvent(), RadioEventType::ERROR, injected); state(f.d, true, false); f.noMetrics();
    TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::READ)); TEST_ASSERT_EQUAL_UINT(0, f.h.count(HardwareCall::FINISH_RX));
}
void test_rx_finish_failure_retains_successful_read_metrics() {
    Fixture f; f.rx(); f.h.finishRxCode = cleanupError; const auto e = f.receiveEvent();
    eventIs(e, RadioEventType::ERROR, cleanupError, true); state(f.d, true, false);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(f.packet, e.bytes, kHeaderSize);
    TEST_ASSERT_EQUAL_FLOAT(f.h.rssi, e.rssi); TEST_ASSERT_EQUAL_FLOAT(f.h.snr, e.snr);
}
void test_rx_finish_failure_overrides_timeout_or_crc() {
    for (uint32_t flags : {radio_irq::timeout, radio_irq::crcError, radio_irq::headerError}) {
        Fixture f; f.rx(); f.h.finishRxCode = cleanupError;
        eventIs(f.receiveEvent(flags), RadioEventType::ERROR, cleanupError); state(f.d, true, false); f.noMetrics();
    }
}
void test_rx_finish_failure_overrides_length_error() {
    Fixture f; f.rx(); f.h.packetLength = kMaxFrameSize + 1; f.h.finishRxCode = cleanupError;
    eventIs(f.receiveEvent(), RadioEventType::ERROR, cleanupError); state(f.d, true, false); f.noMetrics();
}
void test_rx_finish_failure_overrides_read_error() {
    Fixture f; f.rx(); f.h.readCode = injected; f.h.finishRxCode = cleanupError;
    eventIs(f.receiveEvent(), RadioEventType::ERROR, cleanupError); state(f.d, true, false); f.noMetrics();
}
void test_rx_recovery_after_cleanup_failure() {
    Fixture f; f.rx(); f.h.finishRxCode = injected;
    eventIs(f.receiveEvent(), RadioEventType::ERROR, injected, true); state(f.d, true, false);
    f.h.finishRxCode = 0; f.h.signal(0, false);
    TEST_ASSERT_EQUAL_INT16(0, f.d.receive()); state(f.d, true, true, true);
    eventIs(f.receiveEvent(), RadioEventType::RX_DONE, 0, true); state(f.d, true, true);
}
void test_metrics_not_reused_after_previous_success() {
    Fixture f; f.rx(); eventIs(f.receiveEvent(), RadioEventType::RX_DONE, 0, true);
    TEST_ASSERT_EQUAL_INT16(0, f.d.receive()); f.h.resetTrace();
    const auto e = f.receiveEvent(radio_irq::crcError);
    eventIs(e, RadioEventType::CRC_ERROR, radio_status::crcMismatch);
    TEST_ASSERT_EQUAL_FLOAT(0, e.rssi); TEST_ASSERT_EQUAL_FLOAT(0, e.snr); TEST_ASSERT_EQUAL_UINT(0, e.length); f.noMetrics();
}
void test_stop_before_initialization() {
    Fixture f; TEST_ASSERT_EQUAL_INT16(0, f.d.stop()); state(f.d, false, false); order(f.h, {});
}
void test_stop_success_from_rx() {
    Fixture f; f.rx(); TEST_ASSERT_EQUAL_INT16(0, f.d.stop()); state(f.d, true, true); order(f.h, {HardwareCall::STANDBY});
}
void test_stop_success_from_tx_cancels_deadline() {
    Fixture f; f.tx(); TEST_ASSERT_EQUAL_INT16(0, f.d.stop()); state(f.d, true, true);
    f.h.resetTrace(); eventIs(f.d.poll(100 + kTxTimeoutUs), RadioEventType::NONE, 0); order(f.h, {});
}
void test_stop_success_from_idle() {
    Fixture f; f.init(); TEST_ASSERT_EQUAL_INT16(0, f.d.stop()); state(f.d, true, true); order(f.h, {HardwareCall::STANDBY});
}
void test_stop_failure_from_rx_and_successful_stop_recovery() {
    Fixture f; f.rx(); f.h.standbyCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.stop()); state(f.d, true, false);
    f.h.resetTrace(); eventIs(f.d.poll(100), RadioEventType::NONE, 0); order(f.h, {});
    f.h.standbyCode = 0; TEST_ASSERT_EQUAL_INT16(0, f.d.stop()); state(f.d, true, true);
}
void test_stop_failure_from_tx() {
    Fixture f; f.tx(); f.h.standbyCode = injected;
    TEST_ASSERT_EQUAL_INT16(injected, f.d.stop()); state(f.d, true, false);
    f.h.resetTrace(); eventIs(f.d.poll(100 + kTxTimeoutUs), RadioEventType::NONE, 0); order(f.h, {});
}
void test_repeated_rx_tx_transactions_reset_events_and_deadlines() {
    Fixture f; f.init();
    for (uint32_t i = 0; i < 32; ++i) {
        f.h.resetTrace(); f.h.signal(0, false);
        TEST_ASSERT_EQUAL_INT16(0, f.d.receive()); eventIs(f.receiveEvent(), RadioEventType::RX_DONE, 0, true);
        const uint32_t now = UINT32_MAX - 1000 + i * kTxTimeoutUs;
        TEST_ASSERT_EQUAL_INT16(0, f.d.transmit(f.packet, kHeaderSize, now));
        f.h.signal(radio_irq::txDone);
        const auto e = f.d.poll(now + 1); eventIs(e, RadioEventType::TX_DONE, 0);
        TEST_ASSERT_EQUAL_UINT(0, e.length); state(f.d, true, true); TEST_ASSERT_FALSE(f.h.violation);
    }
}
}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_init_success_and_order);
    RUN_TEST(test_init_radio_failure_short_circuits);
    RUN_TEST(test_init_header_failure_short_circuits);
    RUN_TEST(test_init_crc_failure_does_not_enable_irq);
    RUN_TEST(test_failed_init_rejects_operations_and_poll);
    RUN_TEST(test_init_retry_after_failure);
    RUN_TEST(test_repeated_begin_failure_retains_prior_initialized_flag);
    RUN_TEST(test_uninitialized_poll_ignores_irq);
    RUN_TEST(test_receive_before_init);
    RUN_TEST(test_receive_while_transmitting_preserves_tx);
    RUN_TEST(test_receive_start_clears_latch_and_enters_rx);
    RUN_TEST(test_receive_start_failure_and_recovery);
    RUN_TEST(test_receive_restart_while_receiving);
    RUN_TEST(test_transmit_null_buffer);
    RUN_TEST(test_transmit_every_short_length);
    RUN_TEST(test_transmit_oversize_lengths);
    RUN_TEST(test_transmit_maximum_valid_packet_and_order);
    RUN_TEST(test_transmit_before_init);
    RUN_TEST(test_transmit_while_transmitting_preserves_packet_and_deadline);
    RUN_TEST(test_transmit_standby_failure);
    RUN_TEST(test_transmit_start_failure_then_receive_recovery);
    RUN_TEST(test_transmit_from_rx_and_fault);
    RUN_TEST(test_airtime_forwards_length_and_result);
    RUN_TEST(test_idle_poll_does_not_touch_hardware);
    RUN_TEST(test_tx_poll_no_pending_irq);
    RUN_TEST(test_rx_poll_no_pending_irq);
    RUN_TEST(test_latched_irq_with_low_gpio_is_consumed);
    RUN_TEST(test_high_gpio_without_latched_irq_is_consumed);
    RUN_TEST(test_tx_ignores_unrelated_and_zero_flags);
    RUN_TEST(test_tx_done_and_completion_order);
    RUN_TEST(test_tx_hardware_timeout_keeps_idle_after_successful_finish);
    RUN_TEST(test_tx_done_and_timeout_prefers_timeout);
    RUN_TEST(test_tx_finish_failure_and_stop_recovery);
    RUN_TEST(test_tx_finish_failure_overrides_timeout_flag);
    RUN_TEST(test_tx_deadline_immediately_before_expiration);
    RUN_TEST(test_tx_deadline_at_expiration);
    RUN_TEST(test_tx_deadline_immediately_after_expiration);
    RUN_TEST(test_tx_deadline_precedes_pending_tx_done);
    RUN_TEST(test_tx_deadline_standby_failure_and_receive_recovery);
    RUN_TEST(test_tx_deadline_across_uint32_wrap);
    RUN_TEST(test_tx_deadline_after_uint32_wrap);
    RUN_TEST(test_irrelevant_irq_does_not_extend_tx_deadline);
    RUN_TEST(test_rx_minimum_packet_and_call_order);
    RUN_TEST(test_rx_maximum_packet_no_buffer_overrun);
    RUN_TEST(test_rx_timeout_no_payload_or_metrics);
    RUN_TEST(test_rx_crc_error_without_rx_done);
    RUN_TEST(test_rx_header_error_without_valid_header);
    RUN_TEST(test_rx_header_error_with_valid_header_and_rx_done_is_accepted);
    RUN_TEST(test_rx_header_error_with_valid_header_alone_waits);
    RUN_TEST(test_rx_crc_overrides_valid_header_and_rx_done);
    RUN_TEST(test_rx_timeout_precedes_crc_and_data);
    RUN_TEST(test_rx_ignores_nonterminal_flags);
    RUN_TEST(test_rx_every_short_packet_length);
    RUN_TEST(test_rx_oversize_lengths_never_read_buffer);
    RUN_TEST(test_rx_read_failure_propagates_without_metrics);
    RUN_TEST(test_rx_read_crc_failure_is_classified);
    RUN_TEST(test_rx_read_timeout_retains_generic_error_classification);
    RUN_TEST(test_rx_standby_failure_skips_payload_and_finish);
    RUN_TEST(test_rx_finish_failure_retains_successful_read_metrics);
    RUN_TEST(test_rx_finish_failure_overrides_timeout_or_crc);
    RUN_TEST(test_rx_finish_failure_overrides_length_error);
    RUN_TEST(test_rx_finish_failure_overrides_read_error);
    RUN_TEST(test_rx_recovery_after_cleanup_failure);
    RUN_TEST(test_metrics_not_reused_after_previous_success);
    RUN_TEST(test_stop_before_initialization);
    RUN_TEST(test_stop_success_from_rx);
    RUN_TEST(test_stop_success_from_tx_cancels_deadline);
    RUN_TEST(test_stop_success_from_idle);
    RUN_TEST(test_stop_failure_from_rx_and_successful_stop_recovery);
    RUN_TEST(test_stop_failure_from_tx);
    RUN_TEST(test_repeated_rx_tx_transactions_reset_events_and_deadlines);
    return UNITY_END();
}
