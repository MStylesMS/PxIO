#include "mock_transport.h"

bool MockTransport::publish(const std::string& topic,
                             const std::string& payload,
                             int  qos,
                             bool retain) {
    std::lock_guard<std::mutex> lk(mutex_);
    published_.push_back({ topic, payload, qos, retain });
    return true;
}

bool MockTransport::subscribe(const std::string& topic,
                               int /*qos*/,
                               MessageCallback callback) {
    std::lock_guard<std::mutex> lk(mutex_);
    subscriptions_[topic] = std::move(callback);
    return true;
}

bool MockTransport::unsubscribe(const std::string& topic) {
    std::lock_guard<std::mutex> lk(mutex_);
    subscriptions_.erase(topic);
    return true;
}

void MockTransport::inject(const std::string& topic, const std::string& payload) {
    MessageCallback cb;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = subscriptions_.find(topic);
        if (it == subscriptions_.end()) return;
        cb = it->second;
    }
    cb(topic, payload);
}

std::vector<MockTransport::PublishedMessage> MockTransport::published() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return published_;
}

void MockTransport::clearPublished() {
    std::lock_guard<std::mutex> lk(mutex_);
    published_.clear();
}

std::optional<MockTransport::PublishedMessage>
MockTransport::lastPublish(const std::string& topic) const {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto it = published_.rbegin(); it != published_.rend(); ++it)
        if (it->topic == topic) return *it;
    return std::nullopt;
}
