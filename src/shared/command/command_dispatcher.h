#pragma once

#include <string>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include "../mqtt/mqtt_transport.h"
#include "../model/endpoint_registry.h"
#include "../config/config_schema.h"

// Forward declaration
class TelemetryPublisher;

// Receives raw JSON strings from commands_topic, validates them, routes to
// endpoints, and publishes outcome events + warnings following the contract:
//
//   valid envelope  → command_received
//   operation done  → command_success | command_warning | command_failed
//   invalid JSON    → warning published; no command_received emitted
class CommandDispatcher
{
public:
    CommandDispatcher(std::shared_ptr<EndpointRegistry> registry,
                      std::shared_ptr<MqttTransport> transport,
                      const MqttConfig &mqtt_config,
                      std::shared_ptr<TelemetryPublisher> telemetry);

    // Register a callback invoked when reload_config command is received.
    // The callback should return true on success, false on failure.
    void setReloadCallback(std::function<bool()> cb);

    // Called by the transport subscription.  Runs on the MQTT thread.
    void onCommandMessage(const std::string &topic, const std::string &payload);

private:
    void handleWrite(const nlohmann::json &cmd);
    void handleSetMode(const nlohmann::json &cmd);
    void handleRead(const nlohmann::json &cmd);
    void handleRepublishState(const nlohmann::json &cmd);
    void handleReloadConfig(const nlohmann::json &cmd);
    void handlePing(const nlohmann::json &cmd);

    // Emit a command event to events_topic.
    void emitEvent(const std::string &event_name,
                   const std::string &command,
                   const std::string &target,
                   const std::string &request_id,
                   const std::string &message);

    // Emit a warning to warnings_topic.
    void emitWarning(const std::string &level,
                     const std::string &code,
                     const std::string &message,
                     const std::string &target = "");

    // Resolve IO topic for an endpoint (used by read command).
    std::string resolveIoTopic(const std::string &endpoint_name) const;

    std::shared_ptr<EndpointRegistry> registry_;
    std::shared_ptr<MqttTransport> transport_;
    MqttConfig mqtt_config_;
    std::shared_ptr<TelemetryPublisher> telemetry_;
    std::function<bool()> reload_cb_;
};
