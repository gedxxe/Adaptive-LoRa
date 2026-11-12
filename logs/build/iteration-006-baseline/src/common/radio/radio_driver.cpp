#include "radio_driver.h"
#include <SPI.h>
#include "board_pins.h"
#include "radio_config.h"

namespace adaptive_lora {
volatile bool RadioDriver::irq_ = false;
void IRAM_ATTR RadioDriver::onDio1() { irq_ = true; }
RadioDriver::RadioDriver()
    : module_(pins::nss, pins::dio1, pins::reset, pins::busy, SPI,
              SPISettings(phy::spiClockHz, MSBFIRST, SPI_MODE0)), radio_(&module_) {}
int16_t RadioDriver::begin() {
    SPI.begin(pins::sck, pins::miso, pins::mosi, pins::nss);
    int16_t code = radio_.begin(phy::frequencyMhz, phy::bandwidthKhz, phy::dataSf,
        phy::codingRateDenominator, phy::privateSyncWord, phy::txPowerDbm,
        phy::preambleSymbols, phy::tcxoVoltage, phy::useRegulatorLdo);
    if (code == RADIOLIB_ERR_NONE) code = radio_.explicitHeader();
    if (code == RADIOLIB_ERR_NONE) code = radio_.setCRC(2);
    if (code != RADIOLIB_ERR_NONE) { state_ = State::FAULT; return code; }
    radio_.setDio1Action(onDio1);
    initialized_ = true;
    state_ = State::IDLE;
    return code;
}
int16_t RadioDriver::receive() {
    if (!initialized_ || transmitting()) return RADIOLIB_ERR_UNKNOWN;
    irq_ = false;
    // Route CRC/header/timeout events as well as RX_DONE to DIO1.
    const int16_t code = radio_.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF,
        RADIOLIB_IRQ_RX_DEFAULT_FLAGS, RADIOLIB_IRQ_RX_DEFAULT_FLAGS, 0);
    state_ = code == RADIOLIB_ERR_NONE ? State::RX : State::FAULT;
    return code;
}
int16_t RadioDriver::transmit(const uint8_t* bytes, size_t length, uint32_t now) {
    if (!initialized_ || transmitting() || !bytes || length < kHeaderSize || length > kMaxFrameSize)
        return RADIOLIB_ERR_PACKET_TOO_LONG;
    int16_t code = radio_.standby();
    if (code == RADIOLIB_ERR_NONE) {
        irq_ = false;
        code = radio_.startTransmit(bytes, length);
    }
    state_ = code == RADIOLIB_ERR_NONE ? State::TX : State::FAULT;
    txDeadline_ = now + kTxTimeoutUs;
    return code;
}
int16_t RadioDriver::stop() {
    if (!initialized_) return RADIOLIB_ERR_NONE;
    const int16_t code = radio_.standby();
    state_ = code == RADIOLIB_ERR_NONE ? State::IDLE : State::FAULT;
    return code;
}
RadioEvent RadioDriver::poll(uint32_t now) {
    RadioEvent event;
    if (state_ != State::TX && state_ != State::RX) return event;
    // Deadline must also run when DIO1 is stuck high or IRQ reads return no TX_DONE.
    if (state_ == State::TX && deadlineReached(now, txDeadline_)) {
        const int16_t stopped = stop();
        event.type = RadioEventType::ERROR;
        event.code = stopped ? stopped : RADIOLIB_ERR_TX_TIMEOUT;
        return event;
    }
    // A high DIO1 also catches an edge arriving while the flag is consumed.
    const bool signalled = irq_ || digitalRead(pins::dio1);
    irq_ = false;
    if (!signalled) return event;
    // This RadioLib getter has no error return; zero/irrelevant flags are ignored.
    const uint32_t flags = radio_.getIrqFlags();
    if (state_ == State::TX) {
        if (!(flags & (RADIOLIB_SX126X_IRQ_TX_DONE | RADIOLIB_SX126X_IRQ_TIMEOUT))) return event;
        event.code = radio_.finishTransmit();
        state_ = event.code == 0 ? State::IDLE : State::FAULT;
        if (event.code == 0 && (flags & RADIOLIB_SX126X_IRQ_TIMEOUT)) event.code = RADIOLIB_ERR_TX_TIMEOUT;
        event.type = event.code == 0 ? RadioEventType::TX_DONE : RadioEventType::ERROR;
        return event;
    }
    // Match RadioLib readData: HEADER_ERR with a valid header can be stale.
    const bool integrityError = (flags & RADIOLIB_SX126X_IRQ_CRC_ERR) ||
        ((flags & RADIOLIB_SX126X_IRQ_HEADER_ERR) && !(flags & RADIOLIB_SX126X_IRQ_HEADER_VALID));
    if (!(flags & (RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_TIMEOUT)) && !integrityError) return event;
    // Freeze the receive buffer before reading its length and payload.
    event.code = radio_.standby();
    state_ = event.code == 0 ? State::IDLE : State::FAULT;
    if (event.code != 0) { event.type = RadioEventType::ERROR; return event; }
    if (flags & RADIOLIB_SX126X_IRQ_TIMEOUT) {
        event.type = RadioEventType::RX_TIMEOUT;
        event.code = RADIOLIB_ERR_RX_TIMEOUT;
    } else if (integrityError) {
        event.type = RadioEventType::CRC_ERROR;
        event.code = RADIOLIB_ERR_CRC_MISMATCH;
    } else {
        event.length = radio_.getPacketLength();
        if (event.length < kHeaderSize || event.length > sizeof(event.bytes)) {
            event.code = RADIOLIB_ERR_PACKET_TOO_LONG;
            event.type = RadioEventType::ERROR;
        } else {
            event.code = radio_.readData(event.bytes, event.length);
            if (event.code == 0) {
                event.type = RadioEventType::RX_DONE;
                event.rssi = radio_.getRSSI();
                event.snr = radio_.getSNR();
                event.metrics = true;
            } else event.type = event.code == RADIOLIB_ERR_CRC_MISMATCH ? RadioEventType::CRC_ERROR : RadioEventType::ERROR;
        }
    }
    const int16_t finished = radio_.finishReceive();
    if (finished != 0) { event.code = finished; event.type = RadioEventType::ERROR; state_ = State::FAULT; }
    return event;
}
}  // namespace adaptive_lora
