#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <string>
#include <functional>
#include <memory>

class MQTTClient {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& message)>;

    MQTTClient(const std::string& brokerAddress, int port = 1883);
    ~MQTTClient();

    // Connection management
    bool connect(const std::string& clientId);
    bool disconnect();
    bool isConnected() const;

    // Publishing and subscribing
    bool publish(const std::string& topic, const std::string& message);
    bool subscribe(const std::string& topic, MessageCallback callback);
    bool unsubscribe(const std::string& topic);

private:
    std::string brokerAddress;
    int port;
    bool connected;
    // Placeholder for MQTT client handle (e.g., MQTTClient from paho-mqtt-c)
};

#endif // MQTT_CLIENT_H
