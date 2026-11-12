#include "llcc68_hardware.h"
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
              "RadioHardware IRQ bits must match LLCC68 raw flags");

volatile bool Llcc68Hardware::irq_ = false;
void IRAM_ATTR Llcc68Hardware::onDio1() { irq_ = true; }
Llcc68Hardware::Llcc68Hardware()
    : module_(pins::nss, pins::dio1, pins::reset, pins::busy, SPI,
              SPISettings(phy::spiClockHz, MSBFIRST, SPI_MODE0)), radio_(&module_) {}
int16_t Llcc68Hardware::begin() {
    SPI.begin(pins::sck, pins::miso, pins::mosi, pins::nss);
    return radio_.begin(phy::frequencyMhz, phy::bandwidthKhz, phy::dataSf,
        phy::codingRateDenominator, phy::privateSyncWord, phy::txPowerDbm,
        phy::preambleSymbols, phy::tcxoVoltage, phy::useRegulatorLdo);
}
void Llcc68Hardware::enableDio1() { radio_.setDio1Action(onDio1); }
bool Llcc68Hardware::irqSignalled() { return irq_ || digitalRead(pins::dio1); }
int16_t Llcc68Hardware::startReceive() {
    // Preserve the full default RX mask, including CRC/header/timeout signals.
    return radio_.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF,
        RADIOLIB_IRQ_RX_DEFAULT_FLAGS, RADIOLIB_IRQ_RX_DEFAULT_FLAGS, 0);
}
}  // namespace adaptive_lora
