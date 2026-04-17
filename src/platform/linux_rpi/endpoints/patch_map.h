#pragma once

#include "../../../shared/model/endpoint.h"
#include "../gpio/gpio_provider.h"
#include "../../../shared/config/config_schema.h"
#include <string>
#include <memory>
#include <vector>
#include <chrono>

// patch_map compound endpoint.
// Pins may dynamically switch between input and output during a scan to
// detect connection chains. The canonical output is published as a
// dash-separated sorted token (e.g. "1-3-A-C").
class PatchMap : public Endpoint
{
public:
    PatchMap(const CompoundEndpointConfig &cfg,
             std::shared_ptr<GpioProvider> gpio,
             bool global_retain);

    bool init() override;
    void shutdown() override;

    std::string name() const override { return "compound:" + cfg_.name; }
    EndpointType type() const override { return EndpointType::PATCH_MAP; }
    std::string resolvedTopic() const override { return resolved_topic_; }
    bool retainEnabled() const override { return retain_; }
    bool isAvailable() const override { return available_; }

    bool read(std::string &value_out) override;
    bool write(const std::string &value) override;  // not applicable; warns + returns false
    bool setMode(const std::string &mode) override; // not applicable; warns + returns false

    bool pollAndCheckChanged() override;
    std::string lastValue() const override { return last_published_; }
    void setResolvedTopic(const std::string &t) override { resolved_topic_ = t; }

    // Canonical chain string: sort numeric-before-alpha, ascending, uppercase.
    static std::string canonicalize(const std::vector<std::string> &pins);

private:
    std::string runScan();

    CompoundEndpointConfig cfg_;
    std::shared_ptr<GpioProvider> gpio_;
    bool retain_;
    bool available_{false};
    std::string resolved_topic_;
    std::string last_published_;

    std::chrono::steady_clock::time_point last_scan_;
    // Pin label → numeric gpio pin index
    std::vector<std::pair<std::string, unsigned int>> pin_map_;
};
