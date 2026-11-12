#include "radio_driver.h"

namespace adaptive_lora {
int16_t RadioDriver::begin() {
    int16_t code = hardware_.begin();
    if (code == radio_status::none) code = hardware_.explicitHeader();
    if (code == radio_status::none) code = hardware_.setCRC(2);
    if (code != radio_status::none) { state_ = State::FAULT; return code; }
    hardware_.enableDio1();
    initialized_ = true;
    state_ = State::IDLE;
    return code;
}
int16_t RadioDriver::receive() {
    if (!initialized_ || transmitting()) return radio_status::unknown;
    hardware_.clearIrqSignal();
    // Route CRC/header/timeout events as well as RX_DONE to DIO1.
    const int16_t code = hardware_.startReceive();
    state_ = code == radio_status::none ? State::RX : State::FAULT;
    return code;
}
int16_t RadioDriver::transmit(const uint8_t* bytes, size_t length, uint32_t now) {
    if (!initialized_ || transmitting() || !bytes || length < kHeaderSize || length > kMaxFrameSize)
        return radio_status::packetTooLong;
    int16_t code = hardware_.standby();
    if (code == radio_status::none) {
        hardware_.clearIrqSignal();
        code = hardware_.startTransmit(bytes, length);
    }
    state_ = code == radio_status::none ? State::TX : State::FAULT;
    txDeadline_ = now + kTxTimeoutUs;
    return code;
}
int16_t RadioDriver::stop() {
    if (!initialized_) return radio_status::none;
    const int16_t code = hardware_.standby();
    state_ = code == radio_status::none ? State::IDLE : State::FAULT;
    return code;
}
RadioEvent RadioDriver::poll(uint32_t now) {
    RadioEvent event;
    if (state_ != State::TX && state_ != State::RX) return event;
    // Deadline must also run when DIO1 is stuck high or IRQ reads return no TX_DONE.
    if (state_ == State::TX && deadlineReached(now, txDeadline_)) {
        const int16_t stopped = stop();
        event.type = RadioEventType::ERROR;
        event.code = stopped ? stopped : radio_status::txTimeout;
        return event;
    }
    // A high DIO1 also catches an edge arriving while the flag is consumed.
    const bool signalled = hardware_.irqSignalled();
    hardware_.clearIrqSignal();
    if (!signalled) return event;
    // This RadioLib getter has no error return; zero/irrelevant flags are ignored.
    const uint32_t flags = hardware_.getIrqFlags();
    if (state_ == State::TX) {
        if (!(flags & (radio_irq::txDone | radio_irq::timeout))) return event;
        event.code = hardware_.finishTransmit();
        state_ = event.code == 0 ? State::IDLE : State::FAULT;
        if (event.code == 0 && (flags & radio_irq::timeout)) event.code = radio_status::txTimeout;
        event.type = event.code == 0 ? RadioEventType::TX_DONE : RadioEventType::ERROR;
        return event;
    }
    // Match RadioLib readData: HEADER_ERR with a valid header can be stale.
    const bool integrityError = (flags & radio_irq::crcError) ||
        ((flags & radio_irq::headerError) && !(flags & radio_irq::headerValid));
    if (!(flags & (radio_irq::rxDone | radio_irq::timeout)) && !integrityError) return event;
    // Freeze the receive buffer before reading its length and payload.
    event.code = hardware_.standby();
    state_ = event.code == 0 ? State::IDLE : State::FAULT;
    if (event.code != 0) { event.type = RadioEventType::ERROR; return event; }
    if (flags & radio_irq::timeout) {
        event.type = RadioEventType::RX_TIMEOUT;
        event.code = radio_status::rxTimeout;
    } else if (integrityError) {
        event.type = RadioEventType::CRC_ERROR;
        event.code = radio_status::crcMismatch;
    } else {
        event.length = hardware_.getPacketLength();
        if (event.length < kHeaderSize || event.length > sizeof(event.bytes)) {
            event.code = radio_status::packetTooLong;
            event.type = RadioEventType::ERROR;
        } else {
            event.code = hardware_.readData(event.bytes, event.length);
            if (event.code == 0) {
                event.type = RadioEventType::RX_DONE;
                event.rssi = hardware_.getRSSI();
                event.snr = hardware_.getSNR();
                event.metrics = true;
            } else event.type = event.code == radio_status::crcMismatch ? RadioEventType::CRC_ERROR : RadioEventType::ERROR;
        }
    }
    const int16_t finished = hardware_.finishReceive();
    if (finished != 0) { event.code = finished; event.type = RadioEventType::ERROR; state_ = State::FAULT; }
    return event;
}
}  // namespace adaptive_lora
