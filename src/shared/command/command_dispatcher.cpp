#include "command_dispatcher.h"
#include "command_schema.h"
#include "../telemetry/telemetry_publisher.h"
#include "../log/logger.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

// ─── helpers ─────────────────────────────────────────────────────────────────

static std::string iso8601_now()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

static std::string jstr(const json &j, const std::string &key, const std::string &def = "")
{
    return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : def;
}

// ─── construction ─────────────────────────────────────────────────────────────

CommandDispatcher::CommandDispatcher(std::shared_ptr<EndpointRegistry> registry,
                                     std::shared_ptr<MqttTransport> transport,
                                     const MqttConfig &mqtt_config,
                                     std::shared_ptr<TelemetryPublisher> telemetry)
    : registry_(std::move(registry)), transport_(std::move(transport)), mqtt_config_(mqtt_config), telemetry_(std::move(telemetry)) {}

void CommandDispatcher::setReloadCallback(std::function<bool()> cb)
{
    reload_cb_ = std::move(cb);
}

// ─── inbound message ─────────────────────────────────────────────────────────

void CommandDispatcher::onCommandMessage(const std::string & /*topic*/,
                                         const std::string &payload)
{
    json cmd;
    try
    {
        cmd = json::parse(payload);
    }
    catch (const json::parse_error &ex)
    {
        LOG_WARN("CommandDispatcher: invalid JSON — " + std::string(ex.what()));
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_MESSAGE_FORMAT,
                    "Command payload is not valid JSON");
        return;
    }

    if (!cmd.contains("command") || !cmd["command"].is_string())
    {
        LOG_WARN("CommandDispatcher: missing 'command' field");
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_MESSAGE_FORMAT,
                    "Command payload missing required 'command' field");
        return;
    }

    std::string command = cmd["command"].get<std::string>();
    std::string target = jstr(cmd, "target");
    std::string request_id = jstr(cmd, "request_id");

    // Emit command_received for every valid envelope
    emitEvent(EventNames::COMMAND_RECEIVED, command, target, request_id, "Command received");

    LOG_DEBUG("CommandDispatcher: command=" + command + (target.empty() ? "" : " target=" + target));

    if (command == CommandNames::WRITE)
        handleWrite(cmd);
    else if (command == CommandNames::SET_MODE)
        handleSetMode(cmd);
    else if (command == CommandNames::READ)
        handleRead(cmd);
    else if (command == CommandNames::REPUBLISH_STATE)
        handleRepublishState(cmd);
    else if (command == CommandNames::RELOAD_CONFIG)
        handleReloadConfig(cmd);
    else if (command == CommandNames::PING)
        handlePing(cmd);
    else
    {
        LOG_WARN("CommandDispatcher: unknown command: " + command);
        emitWarning(WarningLevels::WARNING, WarningCodes::UNKNOWN_COMMAND,
                    "Unknown command: " + command);
        emitEvent(EventNames::COMMAND_FAILED, command, target, request_id,
                  "Unknown command");
    }
}

// ─── command handlers ────────────────────────────────────────────────────────

void CommandDispatcher::handleWrite(const json &cmd)
{
    std::string target = jstr(cmd, "target");
    std::string request_id = jstr(cmd, "request_id");
    std::string value = jstr(cmd, "value");

    if (target.empty())
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_TARGET,
                    "write command missing target");
        emitEvent(EventNames::COMMAND_FAILED, CommandNames::WRITE, target, request_id,
                  "Missing target");
        return;
    }

    auto ep = registry_->get(target);
    if (!ep)
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_TARGET,
                    "Unknown target: " + target, target);
        emitEvent(EventNames::COMMAND_FAILED, CommandNames::WRITE, target, request_id,
                  "Unknown target");
        return;
    }

    if (!ep->isAvailable())
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::ENDPOINT_UNAVAILABLE,
                    "Endpoint unavailable: " + target, target);
        emitEvent(EventNames::COMMAND_WARNING, CommandNames::WRITE, target, request_id,
                  "Endpoint unavailable");
        return;
    }

    // write() returns false when the endpoint rejects it (e.g. direction is input)
    if (!ep->write(value))
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_MODE,
                    "Write rejected by endpoint (check direction): " + target, target);
        emitEvent(EventNames::COMMAND_WARNING, CommandNames::WRITE, target, request_id,
                  "Write rejected — verify endpoint direction");
        return;
    }

    emitEvent(EventNames::COMMAND_SUCCESS, CommandNames::WRITE, target, request_id,
              "Write executed");
}

void CommandDispatcher::handleSetMode(const json &cmd)
{
    std::string target = jstr(cmd, "target");
    std::string request_id = jstr(cmd, "request_id");
    std::string mode = jstr(cmd, "mode");

    if (target.empty() || mode.empty())
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_MESSAGE_FORMAT,
                    "set_mode requires target and mode fields");
        emitEvent(EventNames::COMMAND_FAILED, CommandNames::SET_MODE, target, request_id,
                  "Missing target or mode");
        return;
    }

    if (mode != "input" && mode != "output")
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_MODE,
                    "Invalid mode value: " + mode + " (must be input or output)", target);
        emitEvent(EventNames::COMMAND_FAILED, CommandNames::SET_MODE, target, request_id,
                  "Invalid mode value");
        return;
    }

    auto ep = registry_->get(target);
    if (!ep)
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_TARGET,
                    "Unknown target: " + target, target);
        emitEvent(EventNames::COMMAND_FAILED, CommandNames::SET_MODE, target, request_id,
                  "Unknown target");
        return;
    }

    if (!ep->setMode(mode))
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_MODE,
                    "set_mode rejected by endpoint: " + target, target);
        emitEvent(EventNames::COMMAND_WARNING, CommandNames::SET_MODE, target, request_id,
                  "set_mode rejected");
        return;
    }

    emitEvent(EventNames::COMMAND_SUCCESS, CommandNames::SET_MODE, target, request_id,
              "Mode set to " + mode);
}

void CommandDispatcher::handleRead(const json &cmd)
{
    std::string target = jstr(cmd, "target");
    std::string request_id = jstr(cmd, "request_id");

    if (target.empty())
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_TARGET,
                    "read command missing target");
        emitEvent(EventNames::COMMAND_FAILED, CommandNames::READ, target, request_id,
                  "Missing target");
        return;
    }

    auto ep = registry_->get(target);
    if (!ep)
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::INVALID_TARGET,
                    "Unknown target: " + target, target);
        emitEvent(EventNames::COMMAND_FAILED, CommandNames::READ, target, request_id,
                  "Unknown target");
        return;
    }

    std::string value;
    if (!ep->read(value))
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::ENDPOINT_UNAVAILABLE,
                    "Read failed on endpoint: " + target, target);
        emitEvent(EventNames::COMMAND_WARNING, CommandNames::READ, target, request_id,
                  "Read failed");
        return;
    }

    // Publish the value to the endpoint's IO topic
    transport_->publish(ep->resolvedTopic(), value,
                        mqtt_config_.qos, ep->retainEnabled());

    emitEvent(EventNames::COMMAND_SUCCESS, CommandNames::READ, target, request_id,
              "Read executed");
}

void CommandDispatcher::handleRepublishState(const json &cmd)
{
    std::string request_id = jstr(cmd, "request_id");
    if (telemetry_)
        telemetry_->publishState();
    emitEvent(EventNames::COMMAND_SUCCESS, CommandNames::REPUBLISH_STATE, "",
              request_id, "State republished");
}

void CommandDispatcher::handleReloadConfig(const json &cmd)
{
    std::string request_id = jstr(cmd, "request_id");

    if (!reload_cb_)
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::CONFIG_RELOAD_FAILED,
                    "reload_config: no reload handler registered");
        emitEvent(EventNames::COMMAND_FAILED, CommandNames::RELOAD_CONFIG, "",
                  request_id, "No reload handler");
        return;
    }

    bool ok = reload_cb_();
    if (!ok)
    {
        emitWarning(WarningLevels::WARNING, WarningCodes::CONFIG_RELOAD_FAILED,
                    "Reload failed; continuing with current config");
        emitEvent(EventNames::COMMAND_WARNING, CommandNames::RELOAD_CONFIG, "",
                  request_id, "Reload failed — current config retained");
        return;
    }

    emitEvent(EventNames::COMMAND_SUCCESS, CommandNames::RELOAD_CONFIG, "",
              request_id, "Config reloaded");
}

void CommandDispatcher::handlePing(const json &cmd)
{
    std::string request_id = jstr(cmd, "request_id");
    emitEvent(EventNames::COMMAND_SUCCESS, CommandNames::PING, "",
              request_id, "pong");
}

// ─── event / warning helpers ─────────────────────────────────────────────────

void CommandDispatcher::emitEvent(const std::string &event_name,
                                  const std::string &command,
                                  const std::string &target,
                                  const std::string &request_id,
                                  const std::string &message)
{
    json j;
    j["timestamp"] = iso8601_now();
    j["type"] = EventTypes::COMMAND;
    j["event"] = event_name;
    j["command"] = command;
    if (!target.empty())
        j["target"] = target;
    if (!request_id.empty())
        j["request_id"] = request_id;
    j["message"] = message;

    transport_->publish(mqtt_config_.events_topic, j.dump(),
                        mqtt_config_.qos, false);
}

void CommandDispatcher::emitWarning(const std::string &level,
                                    const std::string &code,
                                    const std::string &message,
                                    const std::string &target)
{
    json j;
    j["timestamp"] = iso8601_now();
    j["level"] = level;
    j["code"] = code;
    j["message"] = message;
    if (!target.empty())
        j["target"] = target;

    transport_->publish(mqtt_config_.warnings_topic, j.dump(),
                        mqtt_config_.qos, false);
}
