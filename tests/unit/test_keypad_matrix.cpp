#include <gtest/gtest.h>

#include <optional>
#include <unordered_set>

#include "platform/linux_rpi/endpoints/keypad_matrix.h"
#include "platform/linux_rpi/gpio/mock_gpio_provider.h"

namespace {

class MatrixMockGpio : public MockGpioProvider
{
public:
    void pressAt(unsigned int row_pin, unsigned int col_pin)
    {
        pressed_row_ = row_pin;
        pressed_col_ = col_pin;
    }

    void release()
    {
        pressed_row_.reset();
        pressed_col_.reset();
    }

    bool setValue(unsigned int pin, int value) override
    {
        if (row_pins_.count(pin))
        {
            if (value == 0)
                active_row_ = pin;
            else if (active_row_ == pin)
                active_row_.reset();
        }
        return MockGpioProvider::setValue(pin, value);
    }

    int getValue(unsigned int pin) override
    {
        if (pressed_col_ && *pressed_col_ == pin &&
            active_row_ && *active_row_ == *pressed_row_)
            return 0;
        return 1;
    }

    void trackRows(const std::vector<int> &rows)
    {
        row_pins_.insert(rows.begin(), rows.end());
    }

private:
    std::unordered_set<unsigned int> row_pins_;
    std::optional<unsigned int> active_row_;
    std::optional<unsigned int> pressed_row_;
    std::optional<unsigned int> pressed_col_;
};

CompoundEndpointConfig moscowKeypadConfig()
{
    CompoundEndpointConfig cfg;
    cfg.name = "keypad";
    cfg.type = "keypad_matrix";
    cfg.io_topic = "Keypad";
    cfg.rows = {6, 13, 19, 26};
    cfg.cols = {25, 8, 7};
    cfg.keys = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "asterik", "0", "pound"};
    cfg.debounce_cycles = 2;
    return cfg;
}

} // namespace

TEST(KeypadMatrix, PublishesDebouncedKeyDownOncePerPress)
{
    auto gpio = std::make_shared<MatrixMockGpio>();
    gpio->trackRows({6, 13, 19, 26});

    KeypadMatrix keypad(moscowKeypadConfig(), gpio, false);
    ASSERT_TRUE(keypad.init());

    for (int i = 0; i < 2; ++i)
        EXPECT_FALSE(keypad.pollAndCheckChanged());

    gpio->pressAt(6, 25); // key "1"
    EXPECT_FALSE(keypad.pollAndCheckChanged());
    ASSERT_TRUE(keypad.pollAndCheckChanged());
    EXPECT_EQ(keypad.lastValue(), "1");

    for (int i = 0; i < 4; ++i)
        EXPECT_FALSE(keypad.pollAndCheckChanged());

    gpio->release();
    EXPECT_FALSE(keypad.pollAndCheckChanged());

    gpio->pressAt(6, 25);
    EXPECT_FALSE(keypad.pollAndCheckChanged());
    ASSERT_TRUE(keypad.pollAndCheckChanged());
    EXPECT_EQ(keypad.lastValue(), "1");
}

TEST(KeypadMatrix, MapsRowColToConfiguredKeyToken)
{
    auto gpio = std::make_shared<MatrixMockGpio>();
    gpio->trackRows({6, 13, 19, 26});

    KeypadMatrix keypad(moscowKeypadConfig(), gpio, false);
    ASSERT_TRUE(keypad.init());

    gpio->pressAt(19, 8); // row 2, col 1 => "8"
    for (int i = 0; i < 2; ++i)
        (void)keypad.pollAndCheckChanged();
    EXPECT_EQ(keypad.lastValue(), "8");
}
