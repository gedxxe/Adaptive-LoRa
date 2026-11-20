#include <unity.h>
#include <cstring>
#include <initializer_list>
#include "common/radio/sx1262_hardware.h"
#include "common/radio/radio_driver.h"
using namespace adaptive_lora;
using namespace host_adapter;
namespace {
void test_module_constructor_uses_v32_radio_pins_and_spi_settings() {
    Sx1262Hardware h; const auto& s = state();
    TEST_ASSERT_EQUAL_INT(8, s.cs); TEST_ASSERT_EQUAL_INT(14, s.dio);
    TEST_ASSERT_EQUAL_INT(12, s.reset); TEST_ASSERT_EQUAL_INT(13, s.busy);
    TEST_ASSERT_EQUAL_UINT32(1000000, s.spiHz);
    TEST_ASSERT_EQUAL_INT(MSBFIRST, s.bitOrder); TEST_ASSERT_EQUAL_INT(SPI_MODE0, s.mode);
}
void test_begin_selects_v32_tcxo_dcdc_and_restored_research_phy() {
    Sx1262Hardware h; TEST_ASSERT_EQUAL_INT(0, h.begin()); const auto& s = state();
    TEST_ASSERT_EQUAL_INT(9, s.spiSck); TEST_ASSERT_EQUAL_INT(11, s.spiMiso);
    TEST_ASSERT_EQUAL_INT(10, s.spiMosi); TEST_ASSERT_EQUAL_INT(8, s.spiCs);
    TEST_ASSERT_EQUAL_INT(SPI_BEGIN, s.trace[0]); TEST_ASSERT_EQUAL_INT(BEGIN, s.trace[1]);
    TEST_ASSERT_EQUAL_FLOAT(915, s.frequency); TEST_ASSERT_EQUAL_FLOAT(125, s.bandwidth);
    TEST_ASSERT_EQUAL_UINT8(7, s.sf); TEST_ASSERT_EQUAL_UINT8(5, s.cr);
    TEST_ASSERT_EQUAL_UINT8(0x12, s.sync); TEST_ASSERT_EQUAL_INT8(22, s.power);
    TEST_ASSERT_EQUAL_UINT16(8, s.preamble); TEST_ASSERT_EQUAL_FLOAT(1.8f, s.tcxo);
    TEST_ASSERT_EQUAL_FLOAT(140.0f, s.currentLimit);
    TEST_ASSERT_EQUAL_INT(CURRENT_LIMIT, s.trace[2]);
    TEST_ASSERT_FALSE(s.ldo);
}
void test_begin_failure_reaches_actual_driver_without_enabling_irq() {
    Sx1262Hardware h; RadioDriver driver(h); state().codes[BEGIN] = -701;
    TEST_ASSERT_EQUAL_INT(-701, driver.begin());
    TEST_ASSERT_EQUAL_UINT(0, state().calls[CURRENT_LIMIT]); TEST_ASSERT_EQUAL_UINT(0, state().calls[HEADER]);
    TEST_ASSERT_EQUAL_UINT(0, state().calls[IRQ]);
}
void test_current_limit_failure_keeps_radio_inactive() {
    Sx1262Hardware h; RadioDriver driver(h); state().codes[CURRENT_LIMIT] = -708;
    TEST_ASSERT_EQUAL_INT(-708, driver.begin());
    TEST_ASSERT_FALSE(driver.healthy());
    TEST_ASSERT_EQUAL_UINT(0, state().calls[HEADER]); TEST_ASSERT_EQUAL_UINT(0, state().calls[IRQ]);
}
void test_actual_driver_reasserts_header_crc_before_irq_registration() {
    Sx1262Hardware h; RadioDriver driver(h); TEST_ASSERT_EQUAL_INT(0, driver.begin());
    const auto& s = state(); TEST_ASSERT_EQUAL_UINT8(2, s.crc);
    TEST_ASSERT_EQUAL_INT(HEADER, s.trace[3]); TEST_ASSERT_EQUAL_INT(CRC, s.trace[4]);
    TEST_ASSERT_EQUAL_INT(IRQ, s.trace[5]); TEST_ASSERT_NOT_NULL(s.callback);
    TEST_ASSERT_EQUAL_STRING("SX1262_driver_id_not_exposed", driver.chip());
}
void test_header_and_crc_failures_stop_driver_initialization() {
    for (auto call : {HEADER, CRC}) {
        state() = State{}; Sx1262Hardware h; RadioDriver driver(h); state().codes[call] = -702;
        TEST_ASSERT_EQUAL_INT(-702, driver.begin()); TEST_ASSERT_EQUAL_UINT(0, state().calls[IRQ]);
    }
}
void test_dio_callback_only_latches_until_consumed() {
    Sx1262Hardware h; h.clearIrqSignal(); h.enableDio1(); const auto before = state().traceLength;
    state().callback(); TEST_ASSERT_TRUE(h.irqSignalled());
    TEST_ASSERT_EQUAL_UINT(before, state().traceLength); h.clearIrqSignal(); TEST_ASSERT_FALSE(h.irqSignalled());
}
void test_dio_level_survives_software_latch_clear() {
    Sx1262Hardware h; h.clearIrqSignal(); state().dioHigh = true;
    TEST_ASSERT_TRUE(h.irqSignalled()); h.clearIrqSignal(); TEST_ASSERT_TRUE(h.irqSignalled());
    state().dioHigh = false; TEST_ASSERT_FALSE(h.irqSignalled());
}
void test_receive_forwards_infinite_timeout_and_full_error_mask() {
    Sx1262Hardware h; state().codes[RX] = -703; TEST_ASSERT_EQUAL_INT(-703, h.startReceive());
    TEST_ASSERT_EQUAL_HEX32(RADIOLIB_SX126X_RX_TIMEOUT_INF, state().rxTimeout);
    TEST_ASSERT_EQUAL_HEX32(RADIOLIB_IRQ_RX_DEFAULT_FLAGS, state().rxFlags);
    TEST_ASSERT_EQUAL_HEX32(state().rxFlags, state().rxMask); TEST_ASSERT_EQUAL_UINT(0, state().rxLength);
}
void test_transmit_preserves_payload_length_bytes_and_error() {
    Sx1262Hardware h; uint8_t bytes[96];
    for (unsigned i = 0; i < sizeof(bytes); ++i) bytes[i] = static_cast<uint8_t>(i);
    state().codes[TX] = -704; TEST_ASSERT_EQUAL_INT(-704, h.startTransmit(bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT(96, state().txLength); TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, state().txBytes, 96);
}
void test_standby_and_completion_statuses_are_not_discarded() {
    Sx1262Hardware h; state().codes[STANDBY] = -705; state().codes[FINISH_TX] = -706; state().codes[FINISH_RX] = -707;
    TEST_ASSERT_EQUAL_INT(-705, h.standby()); TEST_ASSERT_EQUAL_INT(-706, h.finishTransmit());
    TEST_ASSERT_EQUAL_INT(-707, h.finishReceive());
}
void test_read_forwards_bytes_length_and_integrity_failure() {
    Sx1262Hardware h; state().rxBytes[0] = 0x4c; state().rxBytes[27] = 0x5a; uint8_t bytes[28]{};
    state().codes[READ] = -7; TEST_ASSERT_EQUAL_INT(-7, h.readData(bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT(28, state().readLength); TEST_ASSERT_EQUAL_UINT8(0x4c, bytes[0]); TEST_ASSERT_EQUAL_UINT8(0x5a, bytes[27]);
}
void test_irq_length_metrics_and_airtime_are_forwarded() {
    Sx1262Hardware h; state().irqFlags = 0x42; state().packetLength = 37;
    TEST_ASSERT_EQUAL_HEX32(0x42, h.getIrqFlags()); TEST_ASSERT_EQUAL_UINT(37, h.getPacketLength());
    TEST_ASSERT_EQUAL_FLOAT(-70, h.getRSSI()); TEST_ASSERT_EQUAL_FLOAT(8, h.getSNR());
    TEST_ASSERT_EQUAL_UINT32(12345, h.getTimeOnAir(96)); TEST_ASSERT_EQUAL_UINT(96, state().toaLength);
}
}
void setUp() { state() = State{}; }
void tearDown() { TEST_ASSERT_FALSE(state().overflow); }
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_module_constructor_uses_v32_radio_pins_and_spi_settings);
    RUN_TEST(test_begin_selects_v32_tcxo_dcdc_and_restored_research_phy);
    RUN_TEST(test_begin_failure_reaches_actual_driver_without_enabling_irq);
    RUN_TEST(test_current_limit_failure_keeps_radio_inactive);
    RUN_TEST(test_actual_driver_reasserts_header_crc_before_irq_registration);
    RUN_TEST(test_header_and_crc_failures_stop_driver_initialization);
    RUN_TEST(test_dio_callback_only_latches_until_consumed);
    RUN_TEST(test_dio_level_survives_software_latch_clear);
    RUN_TEST(test_receive_forwards_infinite_timeout_and_full_error_mask);
    RUN_TEST(test_transmit_preserves_payload_length_bytes_and_error);
    RUN_TEST(test_standby_and_completion_statuses_are_not_discarded);
    RUN_TEST(test_read_forwards_bytes_length_and_integrity_failure);
    RUN_TEST(test_irq_length_metrics_and_airtime_are_forwarded);
    return UNITY_END();
}
