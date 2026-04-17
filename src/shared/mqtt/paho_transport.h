#pragma once

#include "mqtt_transport.h"
#include "../config/config_schema.h"

#include <MQTTAsync.h>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Eclipse Paho C async (MQTTAsync) implementation of MqttTransport.
// Callbacks arrive on Paho's internal thread; all shared state is protected
// by mutex.
class PahoTransport : public MqttTransport
{
public:
    explicit PahoTransport(const MqttConfig &config);
    ~PahoTransport() override;

    bool connect() override;
    bool disconnect() override;
    bool isConnected() const override;

    bool publish(const std::string &topic,
                 const std::string &payload,
                 int qos = 1,
                 bool retain = false) override;

    bool subscribe(const std::string &topic,
                   int qos,
                   MessageCallback callback) override;

    bool unsubscribe(const std::string &topic) override;

private:
    // Paho static callbacks
    static void onConnectSuccess(void *ctx, MQTTAsync_successData *data);
    static void onConnectFailure(void *ctx, MQTTAsync_failureData *data);
    static void onConnectionLost(void *ctx, char *cause);
    static int onMessageArrived(void *ctx, char *topicName, int topicLen,
                                MQTTAsync_message *msg);
    static void onDisconnectSuccess(void *ctx, MQTTAsync_successData *data);

    void dispatchMessage(const std::string &topic, const std::string &payload);

    MqttConfig config_;
    MQTTAsync client_{nullptr};

    mutable std::mutex state_mutex_;
    std::condition_variable connect_cv_;
    bool connect_done_{false};
    bool connect_ok_{false};

    std::condition_variable disconnect_cv_;
    bool disconnect_done_{false};

    std::atomic<bool> connected_{false};

    // Topic → subscription callback map
    mutable std::mutex sub_mutex_;
    std::unordered_map<std::string, MessageCallback> subscriptions_;
};
