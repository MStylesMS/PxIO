#pragma once

#include "mqtt_transport.h"

#include <unordered_map>
#include <vector>
#include <tuple>
#include <mutex>
#include <optional>

// In-memory MQTT transport for unit/integration tests.
// publish() records calls; manually injecting a message triggers subscriptions.
class MockTransport : public MqttTransport
{
public:
    struct PublishedMessage
    {
        std::string topic;
        std::string payload;
        int qos;
        bool retain;
    };

    bool connect() override
    {
        connected_ = true;
        return true;
    }
    bool disconnect() override
    {
        connected_ = false;
        return true;
    }
    bool isConnected() const override { return connected_; }

    bool publish(const std::string &topic,
                 const std::string &payload,
                 int qos = 1,
                 bool retain = false) override;

    bool subscribe(const std::string &topic,
                   int qos,
                   MessageCallback callback) override;

    bool unsubscribe(const std::string &topic) override;

    // Test helpers ─────────────────────────────────────────────────────────────

    // Inject an inbound message as if it arrived from the broker.
    void inject(const std::string &topic, const std::string &payload);

    // All outbound publishes since last clear.
    std::vector<PublishedMessage> published() const;
    void clearPublished();

    // Returns the last publish to a specific topic, or nullopt.
    std::optional<PublishedMessage> lastPublish(const std::string &topic) const;

private:
    bool connected_{false};

    mutable std::mutex mutex_;
    std::vector<PublishedMessage> published_;
    std::unordered_map<std::string, MessageCallback> subscriptions_;
};
