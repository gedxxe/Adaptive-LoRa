#include "sx1262_hardware.h"
#include <SPI.h>
#include <esp_timer.h>
#include "board_pins.h"
#include "project_config.h"
#include "radio_config.h"

namespace adaptive_lora {
static_assert(radio_status::none == RADIOLIB_ERR_NONE &&
              radio_status::unknown == RADIOLIB_ERR_UNKNOWN &&
              radio_status::packetTooLong == RADIOLIB_ERR_PACKET_TOO_LONG &&
              radio_status::txTimeout == RADIOLIB_ERR_TX_TIMEOUT &&
              radio_status::rxTimeout == RADIOLIB_ERR_RX_TIMEOUT &&
              radio_status::crcMismatch == RADIOLIB_ERR_CRC_MISMATCH,
              "RadioHardware status values must match pinned RadioLib");
static_assert(radio_irq::txDone == RADIOLIB_SX126X_IRQ_TX_DONE &&
              radio_irq::rxDone == RADIOLIB_SX126X_IRQ_RX_DONE &&
              radio_irq::headerValid == RADIOLIB_SX126X_IRQ_HEADER_VALID &&
              radio_irq::headerError == RADIOLIB_SX126X_IRQ_HEADER_ERR &&
              radio_irq::crcError == RADIOLIB_SX126X_IRQ_CRC_ERR &&
              radio_irq::timeout == RADIOLIB_SX126X_IRQ_TIMEOUT,
              "RadioHardware IRQ bits must match SX1262 raw flags");

static_assert(pins::nss == SS && pins::sck == SCK &&
              pins::miso == MISO && pins::mosi == MOSI,
              "Radio SPI pins must match the Heltec V3 variant");

namespace {
portMUX_TYPE dioMux = portMUX_INITIALIZER_UNLOCKED;
}

bool Sx1262Hardware::irq_ = false;
uint64_t Sx1262Hardware::irqTimeUs_ = 0;
void IRAM_ATTR Sx1262Hardware::onDio1() {
    const auto now = static_cast<uint64_t>(esp_timer_get_time());
    portENTER_CRITICAL_ISR(&dioMux);
    if (!irq_) {
        irqTimeUs_ = now;
        irq_ = true;
    }
    portEXIT_CRITICAL_ISR(&dioMux);
}
Sx1262Hardware::Sx1262Hardware()
    : module_(pins::nss, pins::dio1, pins::reset, pins::busy, SPI,
              SPISettings(phy::spiClockHz, MSBFIRST, SPI_MODE0)), radio_(&module_) {}
int16_t Sx1262Hardware::begin() {
    SPI.begin(pins::sck, pins::miso, pins::mosi, pins::nss);
    // begin() enables DIO2 RF switching and CRC in pinned RadioLib 7.8.1.
    // Explicit oscillator/regulator arguments are board requirements.
    const int16_t code = radio_.begin(phy::frequencyMhz, phy::bandwidthKhz, phy::dataSf,
        phy::codingRateDenominator, phy::privateSyncWord, phy::txPowerDbm,
        phy::preambleSymbols, phy::tcxoVoltage, phy::useRegulatorLdo);
    // RadioLib preserves its 60 mA default across PA selection. Apply the
    // datasheet high-power OCP after begin(), before enabling driver IRQs.
    return code ? code : radio_.setCurrentLimit(phy::txCurrentLimitMa);
}
void Sx1262Hardware::enableDio1() { radio_.setDio1Action(onDio1); }
void Sx1262Hardware::clearIrqSignal() {
    uint64_t unused;
    takeIrqTimestampUs(unused);
}
bool Sx1262Hardware::irqSignalled() {
    portENTER_CRITICAL(&dioMux);
    const bool pending = irq_;
    portEXIT_CRITICAL(&dioMux);
    return pending || digitalRead(pins::dio1);
}
bool Sx1262Hardware::takeIrqTimestampUs(uint64_t& timestamp) {
    portENTER_CRITICAL(&dioMux);
    const bool pending = irq_;
    if (pending) timestamp = irqTimeUs_;
    irq_ = false;
    portEXIT_CRITICAL(&dioMux);
    return pending;
}
int16_t Sx1262Hardware::startReceive() {
    // Preserve the full default RX mask, including CRC/header/timeout signals.
    return radio_.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF,
        RADIOLIB_IRQ_RX_DEFAULT_FLAGS, RADIOLIB_IRQ_RX_DEFAULT_FLAGS, 0);
}
int16_t Sx1262Hardware::startTransmit(const uint8_t* bytes, size_t length) {
#if defined(ADAPTIVE_LORA_SECURE)
    RadioModeConfig_t cfg{};
    cfg.transmit.data = bytes;
    cfg.transmit.len = length;
    const int16_t staged = radio_.stageMode(RADIOLIB_RADIO_MODE_TX, &cfg);
    if (staged != RADIOLIB_ERR_NONE) return staged;
    // Heltec uses DIO2 for the RF switch. Preserve the standard TX mode for
    // the module and use the pinned library's timed SPI transaction.
    module_.setRfSwitchState(Module::MODE_TX);
    // The chip uses 64 timeout ticks per millisecond. Start TX without
    // entering RadioLib's separate unbounded wait for the PA ramp.
    constexpr uint32_t ticks = kTxTimeoutUs * uint64_t(64) / 1000;
    static_assert(ticks > 0 && ticks < 0xFFFFFF);
    const uint8_t timeout[]{uint8_t(ticks >> 16), uint8_t(ticks >> 8), uint8_t(ticks)};
    return module_.SPIwriteStream(RADIOLIB_SX126X_CMD_SET_TX, timeout, sizeof(timeout));
#else
    return radio_.startTransmit(bytes, length);
#endif
}
}  // namespace adaptive_lora
