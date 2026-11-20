#include "sx1262_hardware.h"
#include <SPI.h>
#include "board_pins.h"
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

volatile bool Sx1262Hardware::irq_ = false;
void IRAM_ATTR Sx1262Hardware::onDio1() { irq_ = true; }
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
bool Sx1262Hardware::irqSignalled() { return irq_ || digitalRead(pins::dio1); }
int16_t Sx1262Hardware::startReceive() {
    // Preserve the full default RX mask, including CRC/header/timeout signals.
    return radio_.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF,
        RADIOLIB_IRQ_RX_DEFAULT_FLAGS, RADIOLIB_IRQ_RX_DEFAULT_FLAGS, 0);
}
}  // namespace adaptive_lora
