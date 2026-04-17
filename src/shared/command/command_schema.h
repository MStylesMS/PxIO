#pragma once

// All command, event, and warning identifier strings used in MQTT payloads.
// Single source of truth — no magic strings elsewhere.

namespace CommandNames
{
    constexpr const char *WRITE = "write";
    constexpr const char *SET_MODE = "set_mode";
    constexpr const char *READ = "read";
    constexpr const char *REPUBLISH_STATE = "republish_state";
    constexpr const char *RELOAD_CONFIG = "reload_config";
    constexpr const char *PING = "ping";
}

namespace EventTypes
{
    constexpr const char *COMMAND = "command";
}

namespace EventNames
{
    constexpr const char *COMMAND_RECEIVED = "command_received";
    constexpr const char *COMMAND_SUCCESS = "command_success";
    constexpr const char *COMMAND_FAILED = "command_failed";
    constexpr const char *COMMAND_WARNING = "command_warning";
}

namespace WarningCodes
{
    constexpr const char *INVALID_MESSAGE_FORMAT = "invalid_message_format";
    constexpr const char *UNKNOWN_COMMAND = "unknown_command";
    constexpr const char *INVALID_TARGET = "invalid_target";
    constexpr const char *INVALID_MODE = "invalid_mode";
    constexpr const char *UNSUPPORTED_TYPE = "unsupported_type";
    constexpr const char *CONFIG_RELOAD_FAILED = "config_reload_failed";
    constexpr const char *TRANSPORT_ERROR = "transport_error";
    constexpr const char *ENDPOINT_UNAVAILABLE = "endpoint_unavailable";
    constexpr const char *ENDPOINT_BUSY = "endpoint_busy";
}

namespace WarningLevels
{
    constexpr const char *INFO = "info";
    constexpr const char *WARNING = "warning";
    constexpr const char *ERROR = "error";
    constexpr const char *FATAL = "fatal";
}
