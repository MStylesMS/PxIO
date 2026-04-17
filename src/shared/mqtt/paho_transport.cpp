#include "paho_transport.h"
#include "../log/logger.h"

#include <chrono>
#include <stdexcept>

// ─── construction ─────────────────────────────────────────────────────────────

PahoTransport::PahoTransport(const MqttConfig &config) : config_(config) {}

PahoTransport::~PahoTransport()
{
    if (connected_)
        disconnect();
    if (client_)
        MQTTAsync_destroy(&client_);
}

// ─── connect ─────────────────────────────────────────────────────────────────

bool PahoTransport::connect()
{
    std::string uri = "tcp://" + config_.host + ":" + std::to_string(config_.port);

    int rc = MQTTAsync_create(&client_, uri.c_str(), config_.client_id.c_str(),
                              MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTASYNC_SUCCESS)
    {
        LOG_ERROR("PahoTransport: MQTTAsync_create failed: " + std::to_string(rc));
        return false;
    }

    rc = MQTTAsync_setCallbacks(client_, this, onConnectionLost, onMessageArrived, nullptr);
    if (rc != MQTTASYNC_SUCCESS)
    {
        LOG_ERROR("PahoTransport: setCallbacks failed: " + std::to_string(rc));
        return false;
    }

    MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;
    opts.keepAliveInterval = 60;
    opts.cleansession = 1;
    opts.onSuccess = onConnectSuccess;
    opts.onFailure = onConnectFailure;
    opts.context = this;

    if (!config_.username.empty())
    {
        opts.username = config_.username.c_str();
        opts.password = config_.password.c_str();
    }

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        connect_done_ = false;
        connect_ok_ = false;
    }

    rc = MQTTAsync_connect(client_, &opts);
    if (rc != MQTTASYNC_SUCCESS)
    {
        LOG_ERROR("PahoTransport: MQTTAsync_connect failed: " + std::to_string(rc));
        return false;
    }

    // Wait up to 15 s for connect callback
    std::unique_lock<std::mutex> lk(state_mutex_);
    connect_cv_.wait_for(lk, std::chrono::seconds(15),
                         [this]
                         { return connect_done_; });

    if (!connect_ok_)
        LOG_ERROR("PahoTransport: connection to " + uri + " failed or timed out");

    return connect_ok_;
}

// ─── disconnect ───────────────────────────────────────────────────────────────

bool PahoTransport::disconnect()
{
    if (!connected_)
        return true;

    MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
    opts.onSuccess = onDisconnectSuccess;
    opts.context = this;
    opts.timeout = 5000;

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        disconnect_done_ = false;
    }

    int rc = MQTTAsync_disconnect(client_, &opts);
    if (rc != MQTTASYNC_SUCCESS)
    {
        LOG_WARN("PahoTransport: disconnect request failed: " + std::to_string(rc));
        return false;
    }

    std::unique_lock<std::mutex> lk(state_mutex_);
    disconnect_cv_.wait_for(lk, std::chrono::seconds(10),
                            [this]
                            { return disconnect_done_; });
    return true;
}

bool PahoTransport::isConnected() const { return connected_; }

// ─── publish ──────────────────────────────────────────────────────────────────

bool PahoTransport::publish(const std::string &topic,
                            const std::string &payload,
                            int qos,
                            bool retain)
{
    if (!connected_)
    {
        LOG_WARN("PahoTransport: publish skipped — not connected");
        return false;
    }

    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = const_cast<char *>(payload.c_str());
    msg.payloadlen = static_cast<int>(payload.size());
    msg.qos = qos;
    msg.retained = retain ? 1 : 0;

    MQTTAsync_responseOptions resp = MQTTAsync_responseOptions_initializer;
    int rc = MQTTAsync_sendMessage(client_, topic.c_str(), &msg, &resp);
    if (rc != MQTTASYNC_SUCCESS)
    {
        LOG_WARN("PahoTransport: publish failed on topic " + topic + " rc=" + std::to_string(rc));
        return false;
    }
    return true;
}

// ─── subscribe ────────────────────────────────────────────────────────────────

bool PahoTransport::subscribe(const std::string &topic, int qos, MessageCallback cb)
{
    if (!connected_)
    {
        LOG_WARN("PahoTransport: subscribe skipped — not connected");
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(sub_mutex_);
        subscriptions_[topic] = std::move(cb);
    }

    MQTTAsync_responseOptions resp = MQTTAsync_responseOptions_initializer;
    int rc = MQTTAsync_subscribe(client_, topic.c_str(), qos, &resp);
    if (rc != MQTTASYNC_SUCCESS)
    {
        LOG_WARN("PahoTransport: subscribe failed on topic " + topic + " rc=" + std::to_string(rc));
        return false;
    }
    LOG_INFO("PahoTransport: subscribed to " + topic);
    return true;
}

bool PahoTransport::unsubscribe(const std::string &topic)
{
    {
        std::lock_guard<std::mutex> lk(sub_mutex_);
        subscriptions_.erase(topic);
    }
    MQTTAsync_responseOptions resp = MQTTAsync_responseOptions_initializer;
    MQTTAsync_unsubscribe(client_, topic.c_str(), &resp);
    return true;
}

// ─── static callbacks ─────────────────────────────────────────────────────────

void PahoTransport::onConnectSuccess(void *ctx, MQTTAsync_successData *)
{
    auto *self = static_cast<PahoTransport *>(ctx);
    self->connected_ = true;
    {
        std::lock_guard<std::mutex> lk(self->state_mutex_);
        self->connect_done_ = true;
        self->connect_ok_ = true;
    }
    self->connect_cv_.notify_one();
    LOG_INFO("PahoTransport: connected to broker");
}

void PahoTransport::onConnectFailure(void *ctx, MQTTAsync_failureData *data)
{
    auto *self = static_cast<PahoTransport *>(ctx);
    std::string reason = data ? std::to_string(data->code) : "unknown";
    LOG_ERROR("PahoTransport: connect failure, code=" + reason);
    {
        std::lock_guard<std::mutex> lk(self->state_mutex_);
        self->connect_done_ = true;
        self->connect_ok_ = false;
    }
    self->connect_cv_.notify_one();
}

void PahoTransport::onConnectionLost(void *ctx, char *cause)
{
    auto *self = static_cast<PahoTransport *>(ctx);
    self->connected_ = false;
    std::string reason = cause ? std::string(cause) : "unknown";
    LOG_WARN("PahoTransport: connection lost — " + reason);
}

int PahoTransport::onMessageArrived(void *ctx, char *topicName, int /*topicLen*/,
                                    MQTTAsync_message *msg)
{
    auto *self = static_cast<PahoTransport *>(ctx);
    std::string topic(topicName);
    std::string payload(static_cast<char *>(msg->payload),
                        static_cast<std::size_t>(msg->payloadlen));
    MQTTAsync_freeMessage(&msg);
    MQTTAsync_free(topicName);
    self->dispatchMessage(topic, payload);
    return 1; // 1 = message handled
}

void PahoTransport::onDisconnectSuccess(void *ctx, MQTTAsync_successData *)
{
    auto *self = static_cast<PahoTransport *>(ctx);
    self->connected_ = false;
    {
        std::lock_guard<std::mutex> lk(self->state_mutex_);
        self->disconnect_done_ = true;
    }
    self->disconnect_cv_.notify_one();
    LOG_INFO("PahoTransport: disconnected");
}

// ─── dispatch ─────────────────────────────────────────────────────────────────

void PahoTransport::dispatchMessage(const std::string &topic, const std::string &payload)
{
    MessageCallback cb;
    {
        std::lock_guard<std::mutex> lk(sub_mutex_);
        auto it = subscriptions_.find(topic);
        if (it == subscriptions_.end())
            return;
        cb = it->second;
    }
    cb(topic, payload);
}
