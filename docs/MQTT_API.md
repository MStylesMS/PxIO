# Pio MQTT API Reference

Version: 0.2.0-draft  
Last Updated: 2026-04-16

## Overview

Pio (Paradox IO) uses MQTT for all bridge communication.

Communication model:
- Commands: control systems to Pio
- State: Pio to control systems
- Events: Pio operational and command outcome events
- Warnings: Pio warnings and errors
- Announce: Pio one-time online announcement
- Heartbeat: Pio periodic health status

Design rules:
- IO data topics use simple string payloads whenever possible.
- Bridge control and telemetry topics use JSON payloads.
- Event subtypes are fields in JSON, not second-level topics.

## Topic Structure

### Bridge Topics

Configured by INI values:

- commands_topic: Incoming commands
- state_topic: Bridge state snapshots
- events_topic: Real-time events and command outcomes
- warnings_topic: Warnings and errors
- announce_topic: Online announcement (once per startup)
- heartbeat_topic: Periodic health heartbeat

Example:

paradox/spycatcher/commands
paradox/spycatcher/state
paradox/spycatcher/events
paradox/spycatcher/warnings
paradox/devices

### IO Topic Resolution

Each IO endpoint has io_topic plus a shared base_topic.

Resolution rules:
1. If io_topic starts with /, remove the leading / and treat it as absolute.
2. Otherwise, resolved_topic = base_topic + / + io_topic.

Examples:
- base_topic = paradox/spycatcher, io_topic = keypad
  - resolved_topic = paradox/spycatcher/keypad
- base_topic = paradox/spycatcher, io_topic = /paradox/keypad
  - resolved_topic = paradox/keypad

## Message Formats

### Command Message Format

All commands are JSON and must contain a command field.

{
  "command": "write",
  "target": "gpio:led_red",
  "value": "1",
  "request_id": "abc-123"
}

Fields:
- command: Required command name
- target: Required for target-specific commands
- request_id: Optional caller correlation id

### State Message Format

Published to state_topic.

{
  "timestamp": "2026-04-16T18:30:00.000Z",
  "application": "pio",
  "instance": "rpi-main",
  "status": "running",
  "uptime_s": 120,
  "active_endpoints": 24,
  "qos": 1,
  "global_retain": false
}

### Event Message Format

Published to events_topic.

{
  "timestamp": "2026-04-16T18:30:01.000Z",
  "type": "command",
  "event": "command_success",
  "command": "write",
  "target": "gpio:led_red",
  "request_id": "abc-123",
  "message": "Command executed"
}

Command outcome event values:
- command_received
- command_success
- command_failed
- command_warning

Command outcome event fields:
- timestamp: ISO 8601 timestamp
- type: command
- event: one of command_received, command_success, command_failed, command_warning
- command: command name
- target: optional command target
- request_id: optional caller correlation id
- message: human-readable status summary
- details: optional object with command-specific context

### Warning Message Format

Published to warnings_topic.

{
  "timestamp": "2026-04-16T18:30:02.000Z",
  "level": "warning",
  "code": "unsupported_type",
  "message": "Type touch is not supported on this platform",
  "target": "gpio:touch_1"
}

Warning/error message fields:
- timestamp: ISO 8601 timestamp
- level: info, warning, error, or fatal
- code: stable machine-readable code
- message: human-readable summary
- target: optional endpoint or component name
- details: optional object with structured context

### Announce Message Format

Published once to announce_topic when Pio comes online.

{
  "timestamp": "2026-04-16T18:30:00.000Z",
  "application": "pio",
  "instance": "rpi-main",
  "status": "online",
  "version": "0.1.0",
  "config_file": "pio.ini"
}

### Heartbeat Message Format

Published periodically to heartbeat_topic at heartbeat_interval_ms.

{
  "timestamp": "2026-04-16T18:30:05.000Z",
  "application": "pio",
  "instance": "rpi-main",
  "alive": true,
  "status": "running",
  "uptime_s": 125
}

## Command Reference

Publish all commands to commands_topic.

### write

Write value to output-capable target.

{
  "command": "write",
  "target": "gpio:led_red",
  "value": "1"
}

### set_mode

Switch compatible endpoint direction between input and output.

{
  "command": "set_mode",
  "target": "gpio:line_3",
  "mode": "input"
}

Mode values:
- input
- output

### read

Request immediate read of a target.

{
  "command": "read",
  "target": "gpio:door_switch"
}

### republish_state

Force bridge state snapshot publish.

{
  "command": "republish_state"
}

### reload_config

Reload active INI file.

{
  "command": "reload_config"
}

If reload fails, Pio publishes warning and continues with current active config.

### ping

Health check.

{
  "command": "ping",
  "request_id": "health-001"
}

### Command Processing Behavior

For each command received on commands_topic, Pio publishes command outcome events to events_topic:
1. command_received
2. command_success, command_failed, or command_warning

When command_warning is emitted, command_success may follow if the command completed with non-fatal degradations.

## IO Data Contract

IO data topics publish simple string payloads.

Examples by type:
- digital_in or digital_out: 0 or 1
- integer-like values: 42
- keypad_matrix: A, 7, #
- patch_map: 1-3-5 or 2-A-C

No JSON wrappers are required for normal IO payloads.

## Component-Specific Behavior

### keypad_matrix

Behavior:
- Rows are scanned as outputs.
- Columns are read as inputs.
- Decoded key is published as a single token to the resolved IO topic.

Payload examples:
- 1
- A
- #

### patch_map

Behavior:
- Pins may switch between input/output during scan.
- Connection chains can include more than two members.

Canonical output rules:
1. Sort chain lowest to highest.
2. Numeric members appear before alphabetic members.
3. Alphabetic members are uppercase.
4. Lowercase input is tolerated but normalized.

Canonical examples:
- 1-3-5
- 2-A-C

Non-canonical examples:
- 3-5-1
- c-a

## QoS and Retain

- qos is configurable in INI and defaults to 1.
- global_retain is configurable in INI and defaults to false.
- Per-IO retain override is supported via endpoint retain field.
- Per-IO retain override takes precedence over global_retain.

## Error Handling

Pio publishes warning and error JSON to warnings_topic.

Common warning/error codes:
- invalid_message_format
- unknown_command
- invalid_target
- invalid_mode
- unsupported_type
- config_reload_failed
- transport_error

Unsupported-type behavior requirement:
- publish warning
- log warning
- continue running

## QoS Recommendations

Suggested defaults:

| Message Type | QoS | Rationale |
|--------------|-----|-----------|
| Commands | 1 | Ensure delivery |
| Events | 1 | Important for outcomes and diagnostics |
| Warnings | 1 | Important operational visibility |
| State | 0 or 1 | Depends on update frequency |
| Heartbeat | 0 | Presence-based and periodic |
| IO data | 0 or 1 | Depends on endpoint criticality |

Pio default remains qos = 1 unless overridden in INI.

## Examples

Publish command:

mosquitto_pub -h localhost -t "paradox/spycatcher/commands" -m '{"command":"write","target":"gpio:led_red","value":"1"}'

Subscribe to events:

mosquitto_sub -h localhost -t "paradox/spycatcher/events" -v

Subscribe to warnings:

mosquitto_sub -h localhost -t "paradox/spycatcher/warnings" -v

Subscribe to IO values:

mosquitto_sub -h localhost -t "paradox/spycatcher/#" -v

## Compatibility Notes

This Pio contract follows the established Paradox topic pattern:
- commands
- state
- events
- warnings

Additional announce and heartbeat topics are explicitly configurable to support shared fleet-level monitoring and online presence reporting.
