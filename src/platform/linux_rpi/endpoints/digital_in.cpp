#include "digital_in.h"
#include "../../../shared/log/logger.h"

// ─── topic resolution helper ─────────────────────────────────────────────────

static std::string resolveTopic(const std::string &base_topic,
                                const std::string &io_topic)
{
    if (!io_topic.empty() && io_topic.front() == '/')
        return io_topic.substr(1); // absolute — strip leading /
    return base_topic + "/" + io_topic;
}

// ─── construction ─────────────────────────────────────────────────────────────

DigitalIn::DigitalIn(const GpioEndpointConfig &cfg,
                     std::shared_ptr<GpioProvider> gpio,
                     bool global_retain)
    : cfg_(cfg), gpio_(std::move(gpio)), retain_(cfg.retain_explicit ? cfg.retain : global_retain) {}

// ─── lifecycle ───────────────────────────────────────────────────────────────

bool DigitalIn::init()
{
    // resolved_topic_ needs base_topic — injected separately via the registry
    // builder in main_rpi.cpp.  Store the raw io_topic for now; caller sets it.
    resolved_topic_ = cfg_.io_topic; // overwritten by factory in main_rpi.cpp

    unsigned int pin = static_cast<unsigned int>(std::stoi(cfg_.pin));
    if (!gpio_->requestLine(pin, PinDirection::INPUT))
    {
        LOG_WARN("DigitalIn: failed to request pin " + cfg_.pin + " for endpoint gpio:" + cfg_.name);
        available_ = false;
        return false;
    }
    available_ = true;
    // Prime the debounce with the current value
    int v = gpio_->getValue(pin);
    last_published_ = candidate_value_ = (v >= 1) ? 1 : 0;
    last_published_ = std::to_string(candidate_value_);
    return true;
}

void DigitalIn::shutdown()
{
    if (available_)
    {
        gpio_->releaseLine(static_cast<unsigned int>(std::stoi(cfg_.pin)));
        available_ = false;
    }
}

// ─── IO operations ───────────────────────────────────────────────────────────

bool DigitalIn::read(std::string &value_out)
{
    if (!available_)
        return false;
    int v = gpio_->getValue(static_cast<unsigned int>(std::stoi(cfg_.pin)));
    if (v < 0)
        return false;
    value_out = std::to_string(v);
    return true;
}

bool DigitalIn::write(const std::string & /*value*/)
{
    LOG_WARN("DigitalIn: write rejected — gpio:" + cfg_.name + " is an input endpoint");
    return false;
}

bool DigitalIn::setMode(const std::string &mode)
{
    if (!cfg_.allow_mode_switch)
    {
        LOG_WARN("DigitalIn: set_mode rejected — allow_mode_switch=false on gpio:" + cfg_.name);
        return false;
    }
    unsigned int pin = static_cast<unsigned int>(std::stoi(cfg_.pin));
    PinDirection dir = (mode == "output") ? PinDirection::OUTPUT : PinDirection::INPUT;
    return gpio_->setDirection(pin, dir);
}

// ─── change detection (debounced) ─────────────────────────────────────────────

bool DigitalIn::pollAndCheckChanged()
{
    if (!available_)
        return false;
    unsigned int pin = static_cast<unsigned int>(std::stoi(cfg_.pin));
    int raw = gpio_->getValue(pin);
    if (raw < 0)
        return false;

    if (raw == candidate_value_)
    {
        ++debounce_count_;
    }
    else
    {
        candidate_value_ = raw;
        debounce_count_ = 1;
    }

    if (debounce_count_ >= cfg_.debounce_cycles)
    {
        std::string new_val = std::to_string(candidate_value_);
        if (new_val != last_published_)
        {
            last_published_ = new_val;
            debounce_count_ = 0;
            return true;
        }
    }
    return false;
}
