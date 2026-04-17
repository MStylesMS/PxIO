#include "mcp23017.h"
#include "../../../shared/log/logger.h"

Mcp23017::Mcp23017(const std::string &device, uint8_t address)
    : device_(device), address_(address) {}

Mcp23017::~Mcp23017() { closeChip(); }

// ─── lifecycle ───────────────────────────────────────────────────────────────

bool Mcp23017::openChip(const std::string & /*chip_path*/)
{
    if (!bus_.open(device_, address_))
        return false;

    // Reset all pins to input (IODIR = 0xFF), no pull-ups initially
    if (!bus_.writeByte(IODIRA, 0xFF) || !bus_.writeByte(IODIRB, 0xFF))
    {
        LOG_ERROR("Mcp23017: failed to initialise IODIR registers");
        bus_.close();
        return false;
    }
    iodir_[0] = iodir_[1] = 0xFF;
    olat_[0] = olat_[1] = 0x00;
    LOG_INFO("Mcp23017: opened at " + device_ + " addr=0x" + std::to_string(address_));
    return true;
}

void Mcp23017::closeChip() { bus_.close(); }

// ─── line request ────────────────────────────────────────────────────────────

bool Mcp23017::requestLine(unsigned int pin, PinDirection direction)
{
    if (pin > 15)
    {
        LOG_ERROR("Mcp23017: pin " + std::to_string(pin) + " out of range (0-15)");
        return false;
    }
    return setDirection(pin, direction);
}

void Mcp23017::releaseLine(unsigned int pin)
{
    // Release = set to input (safe default)
    setDirection(pin, PinDirection::INPUT);
}

// ─── direction ───────────────────────────────────────────────────────────────

bool Mcp23017::setDirection(unsigned int pin, PinDirection direction)
{
    if (pin > 15)
        return false;
    uint8_t port = pin / 8;
    uint8_t bit = pin % 8;
    uint8_t reg = (port == 0) ? IODIRA : IODIRB;

    bool is_input = (direction == PinDirection::INPUT);
    if (is_input)
        iodir_[port] |= (1 << bit); // set bit → input
    else
        iodir_[port] &= ~(1 << bit); // clear bit → output

    if (!bus_.writeByte(reg, iodir_[port]))
    {
        LOG_ERROR("Mcp23017: setDirection failed on pin " + std::to_string(pin));
        return false;
    }

    // Enable pull-up for inputs
    uint8_t gppuReg = (port == 0) ? GPPUA : GPPUB;
    int cur = bus_.readByte(gppuReg);
    if (cur >= 0)
    {
        uint8_t gppu = static_cast<uint8_t>(cur);
        if (is_input)
            gppu |= (1 << bit);
        else
            gppu &= ~(1 << bit);
        bus_.writeByte(gppuReg, gppu);
    }
    return true;
}

// ─── read / write ────────────────────────────────────────────────────────────

int Mcp23017::getValue(unsigned int pin)
{
    if (pin > 15)
        return -1;
    uint8_t port = pin / 8;
    uint8_t bit = pin % 8;
    uint8_t reg = (port == 0) ? GPIOA : GPIOB;
    int v = bus_.readByte(reg);
    if (v < 0)
        return -1;
    return (v >> bit) & 0x01;
}

bool Mcp23017::setValue(unsigned int pin, int value)
{
    if (pin > 15)
        return false;
    uint8_t port = pin / 8;
    uint8_t bit = pin % 8;
    uint8_t reg = (port == 0) ? OLATA : OLATB;

    if (value)
        olat_[port] |= (1 << bit);
    else
        olat_[port] &= ~(1 << bit);

    if (!bus_.writeByte(reg, olat_[port]))
    {
        LOG_ERROR("Mcp23017: setValue failed on pin " + std::to_string(pin));
        return false;
    }
    return true;
}

bool Mcp23017::setBit(uint8_t reg, uint8_t bit, bool set)
{
    int cur = bus_.readByte(reg);
    if (cur < 0)
        return false;
    uint8_t v = static_cast<uint8_t>(cur);
    if (set)
        v |= (1 << bit);
    else
        v &= ~(1 << bit);
    return bus_.writeByte(reg, v);
}

bool Mcp23017::getBit(uint8_t reg, uint8_t bit)
{
    int v = bus_.readByte(reg);
    if (v < 0)
        return false;
    return ((v >> bit) & 0x01) != 0;
}
