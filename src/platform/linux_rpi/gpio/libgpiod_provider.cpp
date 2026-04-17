#include "libgpiod_provider.h"
#include "../../../shared/log/logger.h"

#include <cerrno>
#include <cstring>

// ─── helpers ─────────────────────────────────────────────────────────────────

static gpiod_line_settings* makeSettings(PinDirection dir, int output_default = 0) {
    gpiod_line_settings* s = gpiod_line_settings_new();
    if (!s) return nullptr;

    if (dir == PinDirection::OUTPUT) {
        gpiod_line_settings_set_direction(s, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(
            s, output_default ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    } else {
        gpiod_line_settings_set_direction(s, GPIOD_LINE_DIRECTION_INPUT);
        gpiod_line_settings_set_bias(s, GPIOD_LINE_BIAS_PULL_UP);
    }
    return s;
}

// ─── lifecycle ───────────────────────────────────────────────────────────────

LibgpiodProvider::~LibgpiodProvider() {
    closeChip();
}

bool LibgpiodProvider::openChip(const std::string& chip_path) {
    chip_ = gpiod_chip_open(chip_path.c_str());
    if (!chip_) {
        LOG_ERROR("LibgpiodProvider: cannot open chip " + chip_path
                  + ": " + std::strerror(errno));
        return false;
    }
    LOG_INFO("LibgpiodProvider: opened " + chip_path);
    return true;
}

void LibgpiodProvider::closeChip() {
    for (auto& [pin, ls] : lines_) {
        if (ls.request) {
            gpiod_line_request_release(ls.request);
            ls.request = nullptr;
        }
    }
    lines_.clear();

    if (chip_) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
    }
}

// ─── line request ────────────────────────────────────────────────────────────

bool LibgpiodProvider::requestLine(unsigned int pin, PinDirection direction) {
    if (!chip_) {
        LOG_ERROR("LibgpiodProvider: chip not open");
        return false;
    }

    // Release existing request if already claimed
    auto it = lines_.find(pin);
    if (it != lines_.end() && it->second.request) {
        gpiod_line_request_release(it->second.request);
        lines_.erase(it);
    }

    gpiod_line_settings* settings = makeSettings(direction);
    if (!settings) {
        LOG_ERROR("LibgpiodProvider: gpiod_line_settings_new failed for pin "
                  + std::to_string(pin));
        return false;
    }

    gpiod_line_config* line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        gpiod_line_settings_free(settings);
        LOG_ERROR("LibgpiodProvider: gpiod_line_config_new failed");
        return false;
    }

    int rc = gpiod_line_config_add_line_settings(line_cfg, &pin, 1, settings);
    gpiod_line_settings_free(settings);

    if (rc != 0) {
        gpiod_line_config_free(line_cfg);
        LOG_ERROR("LibgpiodProvider: add_line_settings failed for pin "
                  + std::to_string(pin));
        return false;
    }

    gpiod_request_config* req_cfg = gpiod_request_config_new();
    if (req_cfg) gpiod_request_config_set_consumer(req_cfg, "pio");

    gpiod_line_request* req = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);

    if (req_cfg) gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);

    if (!req) {
        LOG_ERROR("LibgpiodProvider: request_lines failed for pin "
                  + std::to_string(pin) + ": " + std::strerror(errno));
        return false;
    }

    lines_[pin] = { req, direction };
    return true;
}

void LibgpiodProvider::releaseLine(unsigned int pin) {
    auto it = lines_.find(pin);
    if (it == lines_.end()) return;
    if (it->second.request)
        gpiod_line_request_release(it->second.request);
    lines_.erase(it);
}

// ─── direction change ────────────────────────────────────────────────────────

bool LibgpiodProvider::setDirection(unsigned int pin, PinDirection direction) {
    auto it = lines_.find(pin);
    if (it == lines_.end() || !it->second.request) {
        LOG_WARN("LibgpiodProvider: setDirection on unrequested pin "
                 + std::to_string(pin));
        return false;
    }
    if (it->second.direction == direction) return true;

    gpiod_line_settings* settings = makeSettings(direction);
    if (!settings) return false;

    gpiod_line_config* line_cfg = gpiod_line_config_new();
    if (!line_cfg) { gpiod_line_settings_free(settings); return false; }

    gpiod_line_config_add_line_settings(line_cfg, &pin, 1, settings);
    gpiod_line_settings_free(settings);

    int rc = gpiod_line_request_reconfigure_lines(it->second.request, line_cfg);
    gpiod_line_config_free(line_cfg);

    if (rc != 0) {
        LOG_ERROR("LibgpiodProvider: reconfigure_lines failed for pin "
                  + std::to_string(pin));
        return false;
    }

    it->second.direction = direction;
    return true;
}

// ─── read / write ────────────────────────────────────────────────────────────

int LibgpiodProvider::getValue(unsigned int pin) {
    auto it = lines_.find(pin);
    if (it == lines_.end() || !it->second.request) {
        LOG_WARN("LibgpiodProvider: getValue on unrequested pin " + std::to_string(pin));
        return -1;
    }
    enum gpiod_line_value v = gpiod_line_request_get_value(it->second.request, pin);
    if (v == GPIOD_LINE_VALUE_ERROR) {
        LOG_WARN("LibgpiodProvider: get_value error on pin " + std::to_string(pin));
        return -1;
    }
    return (v == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
}

bool LibgpiodProvider::setValue(unsigned int pin, int value) {
    auto it = lines_.find(pin);
    if (it == lines_.end() || !it->second.request) {
        LOG_WARN("LibgpiodProvider: setValue on unrequested pin " + std::to_string(pin));
        return false;
    }
    enum gpiod_line_value gv =
        (value != 0) ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    int rc = gpiod_line_request_set_value(it->second.request, pin, gv);
    if (rc != 0) {
        LOG_WARN("LibgpiodProvider: set_value failed on pin " + std::to_string(pin));
        return false;
    }
    return true;
}
