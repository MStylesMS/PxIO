#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "endpoint.h"

// Thread-safe registry of active Endpoint instances indexed by their name
// (e.g. "gpio:led_red", "compound:entry_keypad").
class EndpointRegistry
{
public:
    void add(const std::string &name, std::shared_ptr<Endpoint> endpoint);

    // Returns nullptr if not found.
    std::shared_ptr<Endpoint> get(const std::string &name) const;

    std::vector<std::shared_ptr<Endpoint>> all() const;

    std::size_t size() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Endpoint>> map_;
    std::vector<std::string> order_; // insertion order
};
