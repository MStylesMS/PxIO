#pragma once

#include "../../../shared/model/endpoint.h"
#include "../gpio/gpio_provider.h"
#include "../../../shared/config/config_schema.h"
#include <string>
#include <memory>

// digital_out endpoint: writes to a GPIO pin.
class DigitalOut : public Endpoint
{
public:
    DigitalOut(const GpioEndpointConfig &cfg,
               std::shared_ptr<GpioProvider> gpio,
               bool global_retain);

    bool init() override;
    void shutdown() override;

    std::string name() const override { return "gpio:" + cfg_.name; }
    EndpointType type() const override { return EndpointType::DIGITAL_OUT; }
    std::string resolvedTopic() const override { return resolved_topic_; }
    bool retainEnabled() const override { return retain_; }
    bool isAvailable() const override { return available_; }

    bool read(std::string &value_out) override;
    bool write(const std::string &value) override;
    bool setMode(const std::string &mode) override;

    bool pollAndCheckChanged() override { return false; } // outputs don't auto-publish
    std::string lastValue() const override { return last_value_; }
    void setResolvedTopic(const std::string &t) override { resolved_topic_ = t; }

private:
    GpioEndpointConfig cfg_;
    std::shared_ptr<GpioProvider> gpio_;
    bool retain_;
    bool available_{false};
    std::string resolved_topic_;
    std::string last_value_{"0"};
    Mode current_mode_{Mode::OUTPUT};
};
