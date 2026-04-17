#pragma once

#include "gpio_provider.h"

#include <unordered_map>
#include <vector>
#include <tuple>
#include <optional>

// Configurable in-memory GPIO backend for unit/integration tests.
// No hardware access; all state is held in maps.
class MockGpioProvider : public GpioProvider
{
public:
    struct PinRecord
    {
        bool requested = false;
        PinDirection direction = PinDirection::INPUT;
        int value = 0;
    };

    bool openChip(const std::string & /*chip_path*/) override
    {
        chip_open_ = true;
        return true;
    }
    void closeChip() override
    {
        chip_open_ = false;
        pins_.clear();
    }

    bool requestLine(unsigned int pin, PinDirection direction) override;
    void releaseLine(unsigned int pin) override;
    bool setDirection(unsigned int pin, PinDirection direction) override;
    int getValue(unsigned int pin) override;
    bool setValue(unsigned int pin, int value) override;

    // Test helpers ─────────────────────────────────────────────────────────────

    // Pre-set a pin value that will be returned by getValue().
    void setPresetValue(unsigned int pin, int value);

    // Check what value was last written.
    std::optional<int> lastWrittenValue(unsigned int pin) const;

    // Check requested pins.
    bool isPinRequested(unsigned int pin) const;
    PinDirection pinDirection(unsigned int pin) const;

    // All set-value calls: { pin, value } in order of calls.
    std::vector<std::pair<unsigned int, int>> writeHistory() const;
    void clearHistory();

private:
    bool chip_open_{false};
    std::unordered_map<unsigned int, PinRecord> pins_;
    std::vector<std::pair<unsigned int, int>> write_history_;
};
