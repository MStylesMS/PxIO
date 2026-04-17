#pragma once

#include <string>

enum class PinDirection { INPUT, OUTPUT };

// Digital GPIO abstraction. Platform implementations wrap libgpiod v2 (Linux)
// or equivalent hardware on other targets. MockGpioProvider substitutes in tests.
class GpioProvider {
public:
    virtual ~GpioProvider() = default;

    // Open the GPIO chip (e.g. "/dev/gpiochip0"). Must be called before any
    // line operations.
    virtual bool openChip(const std::string& chip_path) = 0;
    virtual void closeChip() = 0;

    // Claim exclusive access to a single GPIO line.
    virtual bool requestLine(unsigned int pin, PinDirection direction) = 0;
    virtual void releaseLine(unsigned int pin) = 0;

    // Reconfigure direction on an already-requested line.
    virtual bool setDirection(unsigned int pin, PinDirection direction) = 0;

    // Read / write a line value (0 or 1).
    virtual int  getValue(unsigned int pin) = 0;
    virtual bool setValue(unsigned int pin, int value) = 0;
};
