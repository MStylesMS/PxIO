#pragma once

#include <string>
#include <memory>
#include <chrono>
#include "../mqtt/mqtt_transport.h"
#include "../model/endpoint_registry.h"
#include "../config/config_schema.h"

// Publishes all bridge telemetry: announce, heartbeat, state, warnings.
// Heartbeat runs on the caller's thread via tick() — call it from the main loop.
class TelemetryPublisher
{
public:
    TelemetryPublisher(std::shared_ptr<MqttTransport> transport,
                       const MqttConfig &mqtt_config,
                       const GlobalConfig &global_config,
                       std::shared_ptr<EndpointRegistry> registry,
                       const std::string &config_file_path,
                       const std::string &version);

    // Publish the one-time online announcement.
    void publishAnnounce();

    // Publish a full bridge state snapshot.
    void publishState();

    // Publish a warning/error JSON message.
    void publishWarning(const std::string &level,
                        const std::string &code,
                        const std::string &message,
                        const std::string &target = "");

    // Call from main loop to fire heartbeat when its interval has elapsed.
    void tick();

private:
    std::string isoNow() const;
    long uptimeSeconds() const;

    std::shared_ptr<MqttTransport> transport_;
    MqttConfig mqtt_config_;
    GlobalConfig global_config_;
    std::shared_ptr<EndpointRegistry> registry_;
    std::string config_file_path_;
    std::string version_;

    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_heartbeat_;
};
