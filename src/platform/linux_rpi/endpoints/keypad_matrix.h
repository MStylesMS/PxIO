#pragma once

#include "../../../shared/model/endpoint.h"
#include "../gpio/gpio_provider.h"
#include "../../../shared/config/config_schema.h"
#include <string>
#include <memory>
#include <vector>

// keypad_matrix compound endpoint.
// Row pins are driven output; column pins are read as input.
// Matrix is scanned one row at a time; a decoded key token is published.
class KeypadMatrix : public Endpoint
{
public:
    KeypadMatrix(const CompoundEndpointConfig &cfg,
                 std::shared_ptr<GpioProvider> gpio,
                 bool global_retain);

    bool init() override;
    void shutdown() override;

    std::string name() const override { return "compound:" + cfg_.name; }
    EndpointType type() const override { return EndpointType::KEYPAD_MATRIX; }
    std::string resolvedTopic() const override { return resolved_topic_; }
    bool retainEnabled() const override { return retain_; }
    bool isAvailable() const override { return available_; }

    bool read(std::string &value_out) override;
    bool write(const std::string &value) override;  // not applicable; warns + returns false
    bool setMode(const std::string &mode) override; // not applicable; warns + returns false

    bool pollAndCheckChanged() override;
    std::string lastValue() const override { return last_published_; }
    void setResolvedTopic(const std::string &t) override { resolved_topic_ = t; }

private:
    // Scan matrix once; return token or "" if no key pressed.
    std::string scanOnce();

    CompoundEndpointConfig cfg_;
    std::shared_ptr<GpioProvider> gpio_;
    bool retain_;
    bool available_{false};
    std::string resolved_topic_;

    // Debounce
    int debounce_count_{0};
    std::string candidate_key_;
    std::string last_published_;
};
