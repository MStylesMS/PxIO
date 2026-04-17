#include "mock_gpio_provider.h"

bool MockGpioProvider::requestLine(unsigned int pin, PinDirection direction) {
    pins_[pin].requested  = true;
    pins_[pin].direction  = direction;
    return true;
}

void MockGpioProvider::releaseLine(unsigned int pin) {
    auto it = pins_.find(pin);
    if (it != pins_.end()) it->second.requested = false;
}

bool MockGpioProvider::setDirection(unsigned int pin, PinDirection direction) {
    auto it = pins_.find(pin);
    if (it == pins_.end()) return false;
    it->second.direction = direction;
    return true;
}

int MockGpioProvider::getValue(unsigned int pin) {
    auto it = pins_.find(pin);
    return (it != pins_.end()) ? it->second.value : 0;
}

bool MockGpioProvider::setValue(unsigned int pin, int value) {
    pins_[pin].value = value;
    write_history_.push_back({ pin, value });
    return true;
}

void MockGpioProvider::setPresetValue(unsigned int pin, int value) {
    pins_[pin].value = value;
}

std::optional<int> MockGpioProvider::lastWrittenValue(unsigned int pin) const {
    for (auto it = write_history_.rbegin(); it != write_history_.rend(); ++it)
        if (it->first == pin) return it->second;
    return std::nullopt;
}

bool MockGpioProvider::isPinRequested(unsigned int pin) const {
    auto it = pins_.find(pin);
    return (it != pins_.end()) && it->second.requested;
}

PinDirection MockGpioProvider::pinDirection(unsigned int pin) const {
    auto it = pins_.find(pin);
    return (it != pins_.end()) ? it->second.direction : PinDirection::INPUT;
}

std::vector<std::pair<unsigned int, int>> MockGpioProvider::writeHistory() const {
    return write_history_;
}

void MockGpioProvider::clearHistory() {
    write_history_.clear();
}
