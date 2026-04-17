#pragma once

#include <string>
#include <cstdint>

// Thin RAII wrapper around a Linux /dev/i2c-N file descriptor.
class I2cBus
{
public:
    I2cBus() = default;
    ~I2cBus();

    // Open the device and set the default slave address.
    bool open(const std::string &device, uint8_t address);
    void close();

    bool isOpen() const { return fd_ >= 0; }

    // Change the active slave address (I2C_SLAVE ioctl).
    bool setAddress(uint8_t address);

    // Write reg + data byte; return true on success.
    bool writeByte(uint8_t reg, uint8_t data);

    // Read one byte from reg; returns -1 on error.
    int readByte(uint8_t reg);

private:
    int fd_{-1};
    uint8_t address_{0};
};
