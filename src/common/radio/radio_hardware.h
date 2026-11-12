#pragma once
#include <cstddef>
#include <cstdint>

namespace adaptive_lora {
// RadioLib 7.8.1 status values and LLCC68 raw IRQ bits. The production adapter
// asserts these against the library; getters without status cannot report errors.
namespace radio_status {
constexpr int16_t none = 0;
constexpr int16_t unknown = -1;
constexpr int16_t packetTooLong = -4;
constexpr int16_t txTimeout = -5;
constexpr int16_t rxTimeout = -6;
constexpr int16_t crcMismatch = -7;
}
namespace radio_irq {
constexpr uint32_t txDone = 0x0001;
constexpr uint32_t rxDone = 0x0002;
constexpr uint32_t headerValid = 0x0010;
constexpr uint32_t headerError = 0x0020;
constexpr uint32_t crcError = 0x0040;
constexpr uint32_t timeout = 0x0200;
}

// Non-owning driver seam, not a general radio/PHY configuration API. The adapter
// owns fixed-profile setup and IRQ signalling; RadioDriver owns state policy.
class RadioHardware {
public:
    virtual ~RadioHardware() = default;
    virtual int16_t begin() = 0;
    virtual int16_t explicitHeader() = 0;
    virtual int16_t setCRC(uint8_t length) = 0;
    virtual void enableDio1() = 0;
    virtual void clearIrqSignal() = 0;
    virtual bool irqSignalled() = 0;
    virtual int16_t standby() = 0;
    virtual int16_t startReceive() = 0;
    virtual int16_t startTransmit(const uint8_t* bytes, size_t length) = 0;
    virtual int16_t finishTransmit() = 0;
    virtual int16_t finishReceive() = 0;
    virtual uint32_t getIrqFlags() = 0;
    virtual size_t getPacketLength() = 0;
    virtual int16_t readData(uint8_t* bytes, size_t length) = 0;
    virtual float getRSSI() = 0;
    virtual float getSNR() = 0;
    virtual uint32_t getTimeOnAir(size_t length) = 0;
};
}  // namespace adaptive_lora
