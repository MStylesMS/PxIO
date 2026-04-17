#pragma once

#include "gpio_provider.h"
#include <gpiod.h>
#include <unordered_map>
#include <memory>

// libgpiod v2 implementation of GpioProvider.
// Each requested line gets its own gpiod_line_request so that direction
// can be changed independently via setDirection().
class LibgpiodProvider : public GpioProvider {
public:
    LibgpiodProvider() = default;
    ~LibgpiodProvider() override;

    bool openChip(const std::string& chip_path) override;
    void closeChip() override;

    bool requestLine(unsigned int pin, PinDirection direction) override;
    void releaseLine(unsigned int pin) override;

    bool setDirection(unsigned int pin, PinDirection direction) override;
    int  getValue(unsigned int pin) override;
    bool setValue(unsigned int pin, int value) override;

private:
    gpiod_chip* chip_{ nullptr };

    struct LineState {
        gpiod_line_request* request{ nullptr };
        PinDirection        direction{ PinDirection::INPUT };
    };

    std::unordered_map<unsigned int, LineState> lines_;
};
