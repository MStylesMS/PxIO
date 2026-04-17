#include "telemetry_publisher.h"
#include "../log/logger.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;
using clk  = std::chrono::steady_clock;
using sys  = std::chrono::system_clock;

// ─── construction ─────────────────────────────────────────────────────────────

TelemetryPublisher::TelemetryPublisher(std::shared_ptr<MqttTransport>    transport,
                                        const MqttConfig&                 mqtt_config,
                                        const GlobalConfig&               global_config,
                                        std::shared_ptr<EndpointRegistry> registry,
                                        const std::string&                config_file_path,
                                        const std::string&                version)
    : transport_(std::move(transport))
    , mqtt_config_(mqtt_config)
    , global_config_(global_config)
    , registry_(std::move(registry))
    , config_file_path_(config_file_path)
    , version_(version)
    , start_time_(clk::now())
    , last_heartbeat_(clk::now()) {}

// ─── announce ─────────────────────────────────────────────────────────────────

void TelemetryPublisher::publishAnnounce() {
    json j;
    j["timestamp"]   = isoNow();
    j["application"] = "pio";
    j["instance"]    = global_config_.instance_name;
    j["status"]      = "online";
    j["version"]     = version_;
    j["config_file"] = config_file_path_;

    transport_->publish(mqtt_config_.announce_topic, j.dump(),
                        mqtt_config_.qos, false);
    LOG_INFO("TelemetryPublisher: announce published");
}

// ─── state ────────────────────────────────────────────────────────────────────

void TelemetryPublisher::publishState() {
    json j;
    j["timestamp"]        = isoNow();
    j["application"]      = "pio";
    j["instance"]         = global_config_.instance_name;
    j["status"]           = "running";
    j["uptime_s"]         = uptimeSeconds();
    j["active_endpoints"] = static_cast<int>(registry_->size());
    j["qos"]              = mqtt_config_.qos;
    j["global_retain"]    = mqtt_config_.global_retain;

    transport_->publish(mqtt_config_.state_topic, j.dump(),
                        mqtt_config_.qos, false);
}

// ─── warning ─────────────────────────────────────────────────────────────────

void TelemetryPublisher::publishWarning(const std::string& level,
                                         const std::string& code,
                                         const std::string& message,
                                         const std::string& target) {
    json j;
    j["timestamp"] = isoNow();
    j["level"]     = level;
    j["code"]      = code;
    j["message"]   = message;
    if (!target.empty()) j["target"] = target;

    transport_->publish(mqtt_config_.warnings_topic, j.dump(),
                        mqtt_config_.qos, false);
}

// ─── tick (heartbeat) ────────────────────────────────────────────────────────

void TelemetryPublisher::tick() {
    auto now     = clk::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - last_heartbeat_).count();

    if (elapsed < mqtt_config_.heartbeat_interval_ms) return;
    last_heartbeat_ = now;

    json j;
    j["timestamp"]   = isoNow();
    j["application"] = "pio";
    j["instance"]    = global_config_.instance_name;
    j["alive"]       = true;
    j["status"]      = "running";
    j["uptime_s"]    = uptimeSeconds();

    transport_->publish(mqtt_config_.heartbeat_topic, j.dump(), 0, false); // QoS 0 for heartbeat
}

// ─── helpers ─────────────────────────────────────────────────────────────────

std::string TelemetryPublisher::isoNow() const {
    auto now = sys::now();
    auto t   = sys::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

long TelemetryPublisher::uptimeSeconds() const {
    return static_cast<long>(
        std::chrono::duration_cast<std::chrono::seconds>(
            clk::now() - start_time_).count());
}
