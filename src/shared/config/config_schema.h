#pragma once

#include <string>
#include <vector>
#include "../log/logger.h"

// ─── [global] ────────────────────────────────────────────────────────────────

struct GlobalConfig {
    std::string instance_name    = "pio";
    LogLevel    log_level        = LogLevel::INFO;
    int         poll_interval_ms = 20;
    std::string message_level    = "INFO";
    std::string log_file;           // empty = stdout/stderr only
};

// ─── [mqtt] ──────────────────────────────────────────────────────────────────

struct MqttConfig {
    std::string host;
    int         port                  = 1883;
    std::string username;
    std::string password;
    std::string client_id;
    int         qos                   = 1;
    bool        global_retain         = false;
    std::string base_topic;
    std::string commands_topic;
    std::string state_topic;
    std::string events_topic;
    std::string warnings_topic;
    std::string announce_topic;
    std::string heartbeat_topic;
    int         heartbeat_interval_ms = 5000;
};

// ─── [gpio:<name>] ───────────────────────────────────────────────────────────

struct GpioEndpointConfig {
    std::string name;               // e.g. "led_red"
    std::string type;               // "digital_in" | "digital_out"
    std::string pin;                // "17" for native, "i2c_expander_20:0" for expander
    std::string io_topic;
    std::string default_value       = "0";
    bool        retain              = false;
    bool        retain_explicit     = false;  // true if set in INI (overrides global_retain)
    int         debounce_cycles     = 8;
    bool        allow_mode_switch   = false;
};

// ─── [compound:<name>] ───────────────────────────────────────────────────────

struct CompoundEndpointConfig {
    std::string              name;
    std::string              type;           // "keypad_matrix" | "patch_map"
    std::string              io_topic;
    bool                     retain         = false;
    bool                     retain_explicit= false;
    // keypad_matrix
    std::vector<int>         rows;
    std::vector<int>         cols;
    std::vector<std::string> keys;
    int                      debounce_cycles = 16;
    // patch_map
    std::vector<std::string> pins;
    int                      scan_interval_ms = 100;
    bool                     allow_mode_switch = false;
};

// ─── [bus:<name>] ────────────────────────────────────────────────────────────

struct BusConfig {
    std::string name;
    std::string type;           // "i2c" | "mcp23017" | "rs485"
    // i2c
    std::string device;
    // mcp23017
    std::string parent;         // "bus:<parent_name>"
    int         address        = 0;
    int         pin_count      = 16;
    // rs485 (phase 2 placeholder)
    int         baud           = 115200;
};

// ─── aggregate ───────────────────────────────────────────────────────────────

struct AppConfig {
    GlobalConfig                       global;
    MqttConfig                         mqtt;
    std::vector<GpioEndpointConfig>    gpio_endpoints;
    std::vector<CompoundEndpointConfig> compound_endpoints;
    std::vector<BusConfig>             buses;
    std::string                        config_file_path;
};
