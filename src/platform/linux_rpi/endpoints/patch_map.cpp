#include "patch_map.h"
#include "../../../shared/log/logger.h"

#include <algorithm>
#include <cctype>
#include <thread>

using clk = std::chrono::steady_clock;

// ─── construction ─────────────────────────────────────────────────────────────

PatchMap::PatchMap(const CompoundEndpointConfig &cfg,
                   std::shared_ptr<GpioProvider> gpio,
                   bool global_retain)
    : cfg_(cfg), gpio_(std::move(gpio)), retain_(cfg.retain_explicit ? cfg.retain : global_retain), last_scan_(clk::now()) {}

// ─── lifecycle ───────────────────────────────────────────────────────────────

bool PatchMap::init()
{
    resolved_topic_ = cfg_.io_topic; // overwritten by factory

    // Build label → pin index map.  Label is the string from the pins= list;
    // it must be parseable as an integer for native GPIO.
    for (const auto &label : cfg_.pins)
    {
        try
        {
            unsigned int pin = static_cast<unsigned int>(std::stoi(label));
            pin_map_.push_back({label, pin});
        }
        catch (...)
        {
            LOG_WARN("PatchMap: non-numeric pin label '" + label + "' in compound:" + cfg_.name + " — skipped");
        }
    }

    if (pin_map_.size() < 2)
    {
        LOG_WARN("PatchMap: fewer than 2 valid pins for compound:" + cfg_.name);
        available_ = false;
        return false;
    }

    // Initially request all as inputs
    for (const auto &[label, pin] : pin_map_)
    {
        if (!gpio_->requestLine(pin, PinDirection::INPUT))
        {
            LOG_WARN("PatchMap: failed to request pin " + label + " for compound:" + cfg_.name);
            available_ = false;
            return false;
        }
    }

    available_ = true;
    return true;
}

void PatchMap::shutdown()
{
    if (!available_)
        return;
    for (const auto &[label, pin] : pin_map_)
        gpio_->releaseLine(pin);
    available_ = false;
}

// ─── IO operations ───────────────────────────────────────────────────────────

bool PatchMap::read(std::string &value_out)
{
    if (!available_)
        return false;
    value_out = runScan();
    return true;
}

bool PatchMap::write(const std::string & /*value*/)
{
    LOG_WARN("PatchMap: write not supported on compound:" + cfg_.name);
    return false;
}

bool PatchMap::setMode(const std::string & /*mode*/)
{
    LOG_WARN("PatchMap: set_mode not supported on compound:" + cfg_.name);
    return false;
}

// ─── change detection ─────────────────────────────────────────────────────────

bool PatchMap::pollAndCheckChanged()
{
    if (!available_)
        return false;

    auto now = clk::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - last_scan_)
                       .count();
    if (elapsed < cfg_.scan_interval_ms)
        return false;
    last_scan_ = now;

    std::string chain = runScan();
    if (chain != last_published_)
    {
        last_published_ = chain;
        return true;
    }
    return false;
}

// ─── scan ────────────────────────────────────────────────────────────────────

std::string PatchMap::runScan()
{
    // For each pin: drive it LOW (output), check which other pins read LOW (input).
    // Collect all {driver, follower} pairs, then build connected chains.

    std::vector<std::pair<std::string, std::string>> connections;

    for (const auto &[label_d, pin_d] : pin_map_)
    {
        // Set driver pin to output-low
        gpio_->setDirection(pin_d, PinDirection::OUTPUT);
        gpio_->setValue(pin_d, 0);
        std::this_thread::sleep_for(std::chrono::microseconds(20));

        for (const auto &[label_r, pin_r] : pin_map_)
        {
            if (pin_r == pin_d)
                continue;
            if (gpio_->getValue(pin_r) == 0)
                connections.push_back({label_d, label_r});
        }

        // Restore to input
        gpio_->setDirection(pin_d, PinDirection::INPUT);
    }

    if (connections.empty())
        return "";

    // Build the set of all connected labels
    std::vector<std::string> members;
    for (const auto &[a, b] : connections)
    {
        auto addIfAbsent = [&](const std::string &s)
        {
            if (std::find(members.begin(), members.end(), s) == members.end())
                members.push_back(s);
        };
        addIfAbsent(a);
        addIfAbsent(b);
    }

    return canonicalize(members);
}

// ─── canonicalize ─────────────────────────────────────────────────────────────

std::string PatchMap::canonicalize(const std::vector<std::string> &pins)
{
    std::vector<std::string> parts;
    parts.reserve(pins.size());
    for (const auto &p : pins)
    {
        std::string up = p;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        parts.push_back(up);
    }

    // Sort: numeric labels before alpha labels; within each group, ascending.
    std::sort(parts.begin(), parts.end(), [](const std::string &a, const std::string &b)
              {
        bool a_num = !a.empty() && std::isdigit(static_cast<unsigned char>(a[0]));
        bool b_num = !b.empty() && std::isdigit(static_cast<unsigned char>(b[0]));
        if (a_num != b_num) return a_num > b_num; // numerics first
        if (a_num)          return std::stoi(a) < std::stoi(b);
        return a < b; });

    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        if (i)
            out += '-';
        out += parts[i];
    }
    return out;
}
