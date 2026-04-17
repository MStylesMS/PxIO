#include "keypad_matrix.h"
#include "../../../shared/log/logger.h"

#include <chrono>
#include <thread>

KeypadMatrix::KeypadMatrix(const CompoundEndpointConfig& cfg,
                           std::shared_ptr<GpioProvider> gpio,
                           bool global_retain)
    : cfg_(cfg)
    , gpio_(std::move(gpio))
    , retain_(cfg.retain_explicit ? cfg.retain : global_retain) {}

bool KeypadMatrix::init() {
    resolved_topic_ = cfg_.io_topic; // overwritten by factory in main_rpi.cpp

    // Rows → output
    for (int r : cfg_.rows) {
        if (!gpio_->requestLine(static_cast<unsigned int>(r), PinDirection::OUTPUT)) {
            LOG_WARN("KeypadMatrix: failed to request row pin " + std::to_string(r)
                     + " for compound:" + cfg_.name);
            available_ = false;
            return false;
        }
        gpio_->setValue(static_cast<unsigned int>(r), 1); // idle high
    }

    // Cols → input
    for (int c : cfg_.cols) {
        if (!gpio_->requestLine(static_cast<unsigned int>(c), PinDirection::INPUT)) {
            LOG_WARN("KeypadMatrix: failed to request col pin " + std::to_string(c)
                     + " for compound:" + cfg_.name);
            available_ = false;
            return false;
        }
    }

    available_ = true;
    return true;
}

void KeypadMatrix::shutdown() {
    if (!available_) return;
    for (int r : cfg_.rows)
        gpio_->releaseLine(static_cast<unsigned int>(r));
    for (int c : cfg_.cols)
        gpio_->releaseLine(static_cast<unsigned int>(c));
    available_ = false;
}

bool KeypadMatrix::read(std::string& value_out) {
    if (!available_) return false;
    value_out = scanOnce();
    return true;
}

bool KeypadMatrix::write(const std::string& /*value*/) {
    LOG_WARN("KeypadMatrix: write not supported on compound:" + cfg_.name);
    return false;
}

bool KeypadMatrix::setMode(const std::string& /*mode*/) {
    LOG_WARN("KeypadMatrix: set_mode not supported on compound:" + cfg_.name);
    return false;
}

// ─── change detection ────────────────────────────────────────────────────────

bool KeypadMatrix::pollAndCheckChanged() {
    if (!available_) return false;
    std::string key = scanOnce();

    if (key == candidate_key_) {
        ++debounce_count_;
    } else {
        candidate_key_  = key;
        debounce_count_ = 1;
    }

    if (debounce_count_ >= cfg_.debounce_cycles && !key.empty()
            && key != last_published_) {
        last_published_  = key;
        debounce_count_  = 0;
        return true;
    }
    return false;
}

// ─── matrix scan ─────────────────────────────────────────────────────────────

std::string KeypadMatrix::scanOnce() {
    for (std::size_t ri = 0; ri < cfg_.rows.size(); ++ri) {
        unsigned int row_pin = static_cast<unsigned int>(cfg_.rows[ri]);

        // Drive this row LOW
        gpio_->setValue(row_pin, 0);
        // Brief settle — on real hardware a small delay improves reliability
        std::this_thread::sleep_for(std::chrono::microseconds(10));

        for (std::size_t ci = 0; ci < cfg_.cols.size(); ++ci) {
            unsigned int col_pin = static_cast<unsigned int>(cfg_.cols[ci]);
            if (gpio_->getValue(col_pin) == 0) {
                // Key pressed at (ri, ci)
                gpio_->setValue(row_pin, 1); // restore before returning
                std::size_t idx = ri * cfg_.cols.size() + ci;
                if (idx < cfg_.keys.size())
                    return cfg_.keys[idx];
            }
        }

        // Restore row HIGH
        gpio_->setValue(row_pin, 1);
    }
    return ""; // no key pressed
}
