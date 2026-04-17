#pragma once

#include <string>
#include <functional>

// Platform-independent MQTT transport abstraction.
// All MQTT interactions go through this interface so the Paho library is
// never referenced outside of PahoTransport, and MockTransport can be
// substituted for tests without any broker.
class MqttTransport
{
public:
    using MessageCallback = std::function<void(const std::string &topic,
                                               const std::string &payload)>;

    virtual ~MqttTransport() = default;

    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;

    // publish — qos and retain override the library defaults for this call.
    virtual bool publish(const std::string &topic,
                         const std::string &payload,
                         int qos = 1,
                         bool retain = false) = 0;

    // subscribe — the callback is invoked from the transport's internal
    // delivery thread; implementations must be thread-safe.
    virtual bool subscribe(const std::string &topic,
                           int qos,
                           MessageCallback callback) = 0;

    virtual bool unsubscribe(const std::string &topic) = 0;
};
