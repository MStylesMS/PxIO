#pragma once

#include "i2c_bus.h"
#include "../gpio/gpio_provider.h"
#include <string>

// MCP23017 16-bit I2C GPIO expander — implements GpioProvider so that
// endpoints can use expander pins identically to native Pi GPIO pins.
//
// Pins 0-7  map to port A (GPIOA).
// Pins 8-15 map to port B (GPIOB).
//
// The expander uses its own address-scoped I2cBus. The parent I2C bus device
// (e.g. /dev/i2c-1) and the 7-bit hardware address (e.g. 0x20) must be set
// at construction.
class Mcp23017 : public GpioProvider
{
public:
    Mcp23017(const std::string &device, uint8_t address);
    ~Mcp23017() override;

    // GpioProvider — chip_path is ignored for expanders; uses device/address
    // set at construction. Call openChip("") to initialise.
    bool openChip(const std::string &chip_path) override;
    void closeChip() override;

    bool requestLine(unsigned int pin, PinDirection direction) override;
    void releaseLine(unsigned int pin) override;
    bool setDirection(unsigned int pin, PinDirection direction) override;
    int getValue(unsigned int pin) override;
    bool setValue(unsigned int pin, int value) override;

private:
    // MCP23017 register addresses (IOCON.BANK = 0, default)
    static constexpr uint8_t IODIRA = 0x00;
    static constexpr uint8_t IODIRB = 0x01;
    static constexpr uint8_t GPPUA = 0x0C;
    static constexpr uint8_t GPPUB = 0x0D;
    static constexpr uint8_t GPIOA = 0x12;
    static constexpr uint8_t GPIOB = 0x13;
    static constexpr uint8_t OLATA = 0x14;
    static constexpr uint8_t OLATB = 0x15;

    bool setBit(uint8_t reg, uint8_t bit, bool set);
    bool getBit(uint8_t reg, uint8_t bit);

    std::string device_;
    uint8_t address_;
    I2cBus bus_;

    uint8_t iodir_[2] = {0xFF, 0xFF}; // cached IODIRA/B  (1=input)
    uint8_t olat_[2] = {0x00, 0x00};  // cached OLATA/B
};
