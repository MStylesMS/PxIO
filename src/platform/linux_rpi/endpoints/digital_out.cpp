#include "digital_out.h"
#include "../../../shared/log/logger.h"

DigitalOut::DigitalOut(const GpioEndpointConfig &cfg,
                       std::shared_ptr<GpioProvider> gpio,
                       bool global_retain)
    : cfg_(cfg), gpio_(std::move(gpio)), retain_(cfg.retain_explicit ? cfg.retain : global_retain) {}

bool DigitalOut::init()
{
    resolved_topic_ = cfg_.io_topic; // overwritten by factory in main_rpi.cpp

    unsigned int pin = static_cast<unsigned int>(std::stoi(cfg_.pin));
    if (!gpio_->requestLine(pin, PinDirection::OUTPUT))
    {
        LOG_WARN("DigitalOut: failed to request pin " + cfg_.pin + " for endpoint gpio:" + cfg_.name);
        available_ = false;
        return false;
    }
    available_ = true;

    // Apply configured default value
    int def = (cfg_.default_value == "1") ? 1 : 0;
    gpio_->setValue(pin, def);
    last_value_ = std::to_string(def);
    return true;
}

void DigitalOut::shutdown()
{
    if (available_)
    {
        gpio_->setValue(static_cast<unsigned int>(std::stoi(cfg_.pin)), 0); // safe low
        gpio_->releaseLine(static_cast<unsigned int>(std::stoi(cfg_.pin)));
        available_ = false;
    }
}

bool DigitalOut::read(std::string &value_out)
{
    value_out = last_value_;
    return available_;
}

bool DigitalOut::write(const std::string &value)
{
    if (!available_)
        return false;
    if (current_mode_ != Mode::OUTPUT)
    {
        LOG_WARN("DigitalOut: write rejected — endpoint gpio:" + cfg_.name + " is currently in input mode");
        return false;
    }
    int v = (value == "1" || value == "true") ? 1 : 0;
    unsigned int pin = static_cast<unsigned int>(std::stoi(cfg_.pin));
    if (!gpio_->setValue(pin, v))
        return false;
    last_value_ = std::to_string(v);
    return true;
}

bool DigitalOut::setMode(const std::string &mode)
{
    if (!cfg_.allow_mode_switch)
    {
        LOG_WARN("DigitalOut: set_mode rejected — allow_mode_switch=false on gpio:" + cfg_.name);
        return false;
    }
    PinDirection dir;
    if (mode == "input")
    {
        dir = PinDirection::INPUT;
        current_mode_ = Mode::INPUT;
    }
    else
    {
        dir = PinDirection::OUTPUT;
        current_mode_ = Mode::OUTPUT;
    }
    return gpio_->setDirection(static_cast<unsigned int>(std::stoi(cfg_.pin)), dir);
}
