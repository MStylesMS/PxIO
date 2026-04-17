#pragma once

#include <string>
#include "component_types.h"

// Abstract interface for all IO endpoints — shared, no platform dependencies.
class Endpoint
{
public:
    virtual ~Endpoint() = default;

    // Lifecycle
    virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Identity
    virtual std::string name() const = 0;
    virtual EndpointType type() const = 0;
    virtual std::string resolvedTopic() const = 0;
    virtual bool retainEnabled() const = 0;
    virtual bool isAvailable() const = 0;

    // IO operations
    // read()  — immediately sample the endpoint and return its string representation.
    // write() — set output value; returns false on error or if direction is wrong.
    // setMode() — switch compatible endpoint between "input" and "output".
    virtual bool read(std::string &value_out) = 0;
    virtual bool write(const std::string &value) = 0;
    virtual bool setMode(const std::string &mode) = 0;

    // Polled change detection (called by main loop on each tick).
    // Implementations apply internal debounce and return true only when
    // the debounced value differs from the last published value.
    virtual bool pollAndCheckChanged() = 0;
    virtual std::string lastValue() const = 0;

    // Allow the factory in main to overwrite the stored resolved topic after
    // applying base_topic + io_topic resolution rules.
    virtual void setResolvedTopic(const std::string &t) = 0;
};
