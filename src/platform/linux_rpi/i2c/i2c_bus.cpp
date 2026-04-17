#include "i2c_bus.h"
#include "../../../shared/log/logger.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cerrno>
#include <cstring>

I2cBus::~I2cBus() { close(); }

bool I2cBus::open(const std::string& device, uint8_t address) {
    fd_ = ::open(device.c_str(), O_RDWR);
    if (fd_ < 0) {
        LOG_ERROR("I2cBus: cannot open " + device + ": " + std::strerror(errno));
        return false;
    }
    if (!setAddress(address)) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    address_ = address;
    return true;
}

void I2cBus::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool I2cBus::setAddress(uint8_t address) {
    if (fd_ < 0) return false;
    if (ioctl(fd_, I2C_SLAVE, address) < 0) {
        LOG_ERROR("I2cBus: I2C_SLAVE ioctl failed for address 0x"
                  + std::to_string(address) + ": " + std::strerror(errno));
        return false;
    }
    address_ = address;
    return true;
}

bool I2cBus::writeByte(uint8_t reg, uint8_t data) {
    if (fd_ < 0) return false;
    uint8_t buf[2] = { reg, data };
    ssize_t n = write(fd_, buf, 2);
    if (n != 2) {
        LOG_WARN("I2cBus: writeByte reg=0x" + std::to_string(reg) + " failed: "
                 + std::strerror(errno));
        return false;
    }
    return true;
}

int I2cBus::readByte(uint8_t reg) {
    if (fd_ < 0) return -1;
    if (write(fd_, &reg, 1) != 1) {
        LOG_WARN("I2cBus: register select failed for reg=0x" + std::to_string(reg));
        return -1;
    }
    uint8_t val;
    if (read(fd_, &val, 1) != 1) {
        LOG_WARN(std::string("I2cBus: readByte failed: ") + std::strerror(errno));
        return -1;
    }
    return static_cast<int>(val);
}
