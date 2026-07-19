# PxIO INI Configuration Guide

Version: 0.2.0-draft  
Last Updated: 2026-04-16

## Overview

PxIO uses INI files for bridge configuration.

INI responsibilities:
- MQTT broker and topic settings
- Bridge runtime behavior
- IO endpoint definitions
- Compound component definitions
- Bus/provider mappings

Design rules:
- One config file per bridge instance.
- Section naming uses colon format (for example, [gpio:led_red]).
- Dot nesting is not used for section names.

## File Location and Startup

Default config file:
- ./pxio.ini

CLI override:

pxio --config /etc/pxio/bridge-a.ini
pxio -c /etc/pxio/bridge-a.ini

Startup behavior:
- If config file is missing or invalid, PxIO exits with fatal error.
- Active config path is included in startup logs.
- Active config path is included in online announcement payload.

## INI Syntax

```ini
# Comments begin with # or ;

[section]
key = value

[gpio:led_red]
type = digital_out
pin = 17
io_topic = gpio/led_red
```

Rules:
- Booleans: true or false
- Integers: decimal unless explicitly noted
- Arrays: comma-separated values
- Strings: unquoted by default

## Section Naming Convention

Common sections:
- [global]
- [mqtt]
- [gpio:<name>]
- [compound:<name>]
- [bus:<name>]

Examples:
- [gpio:door_switch]
- [compound:entry_keypad]
- [bus:i2c_main]

## Configuration Sections

### [global]

Bridge-level runtime settings.

| Setting | Type | Required | Default | Description |
|---|---|---|---|---|
| instance_name | string | No | pxio | Instance id used in telemetry |
| log_level | string | No | INFO | DEBUG, INFO, WARN, ERROR |
| poll_interval_ms | integer | No | 20 | Main loop interval |
| message_level | string | No | INFO | INFO or DEBUG event verbosity |

Example:

```ini
[global]
instance_name = rpi-main
log_level = INFO
poll_interval_ms = 20
message_level = INFO
```

### [mqtt]

MQTT connection and contract topics.

| Setting | Type | Required | Default | Description |
|---|---|---|---|---|
| host | string | Yes | N/A | Broker host or IP |
| port | integer | No | 1883 | Broker port |
| username | string | No | (none) | MQTT username |
| password | string | No | (none) | MQTT password |
| client_id | string | Yes | N/A | MQTT client id |
| qos | integer | No | 1 | Default publish QoS (0..2) |
| global_retain | boolean | No | false | Default retain behavior for IO topics |
| base_topic | string | Yes | N/A | Prefix for relative io_topic values |
| commands_topic | string | Yes | N/A | PxIO command input topic |
| state_topic | string | Yes | N/A | PxIO state output topic |
| events_topic | string | Yes | N/A | PxIO event output topic |
| warnings_topic | string | Yes | N/A | PxIO warnings/errors topic |
| announce_topic | string | Yes | N/A | One-shot online announcement topic |
| heartbeat_topic | string | Yes | N/A | Periodic heartbeat topic |
| heartbeat_interval_ms | integer | No | 5000 | Heartbeat period in ms |

Example:

```ini
[mqtt]
host = 127.0.0.1
port = 1883
client_id = pxio-rpi-main
qos = 1
global_retain = false
base_topic = paradox/spycatcher
commands_topic = paradox/spycatcher/commands
state_topic = paradox/spycatcher/state
events_topic = paradox/spycatcher/events
warnings_topic = paradox/spycatcher/warnings
announce_topic = paradox/devices
heartbeat_topic = paradox/devices
heartbeat_interval_ms = 5000
```

### [gpio:<name>]

Primitive IO endpoints.

| Setting | Type | Required | Default | Description |
|---|---|---|---|---|
| type | string | Yes | N/A | digital_in or digital_out |
| pin | string/integer | Yes | N/A | Physical or logical pin id |
| io_topic | string | Yes | N/A | Relative or absolute topic |
| default | string/integer | No | 0 | Startup value for output types |
| retain | boolean | No | inherit | Per-IO override of global_retain |
| debounce_cycles | integer | No | 8 | Input debounce cycles |
| allow_mode_switch | boolean | No | false | Allows runtime set_mode command |

Digital output example:

```ini
[gpio:led_red]
type = digital_out
pin = 17
io_topic = gpio/led_red
default = 0
retain = true
allow_mode_switch = true
```

Digital input example:

```ini
[gpio:door_switch]
type = digital_in
pin = 27
io_topic = gpio/door
retain = false
debounce_cycles = 8
allow_mode_switch = true
```

IO topic resolution:
- If io_topic starts with /, it is absolute and ignores base_topic.
- Otherwise, resolved topic is base_topic + / + io_topic.

### [compound:<name>]

Compound endpoint definitions.

#### keypad_matrix

```ini
[compound:entry_keypad]
type = keypad_matrix
rows = 8,9,10,11
cols = 12,13,14,15
keys = 1,2,3,A,4,5,6,B,7,8,9,C,*,0,#,D
io_topic = keypad/entry
retain = false
debounce_cycles = 16
```

Rules:
- rows and cols are explicitly configured
- key payload is published as simple string

#### patch_map

```ini
[compound:main_patch]
type = patch_map
pins = 1,2,3,4,5,A,B,C
io_topic = patch/main
retain = false
scan_interval_ms = 100
```

Rules:
- pins may be switched dynamically between input/output during scan
- chain output is canonicalized (ascending, numeric before alpha, alpha uppercase)

### [bus:<name>]

Bus/provider definitions.

I2C bus example:

```ini
[bus:i2c_main]
type = i2c
device = /dev/i2c-1
```

I2C expander example:

```ini
[bus:i2c_expander_20]
type = mcp23017
parent = bus:i2c_main
address = 0x20
pin_count = 16
```

RS-485 placeholder (future phase):

```ini
[bus:rs485_main]
type = rs485
device = /dev/ttyUSB0
baud = 115200
```

## Complete Example

```ini
[global]
instance_name = rpi-main
log_level = INFO
poll_interval_ms = 20
message_level = INFO

[mqtt]
host = 127.0.0.1
port = 1883
client_id = pxio-rpi-main
qos = 1
global_retain = false
base_topic = paradox/spycatcher
commands_topic = paradox/spycatcher/commands
state_topic = paradox/spycatcher/state
events_topic = paradox/spycatcher/events
warnings_topic = paradox/spycatcher/warnings
announce_topic = paradox/devices
heartbeat_topic = paradox/devices
heartbeat_interval_ms = 5000

[gpio:led_red]
type = digital_out
pin = 17
io_topic = gpio/led_red
default = 0
retain = true
allow_mode_switch = true

[gpio:door_switch]
type = digital_in
pin = 27
io_topic = gpio/door
debounce_cycles = 8
allow_mode_switch = true

[compound:entry_keypad]
type = keypad_matrix
rows = 8,9,10,11
cols = 12,13,14,15
keys = 1,2,3,A,4,5,6,B,7,8,9,C,*,0,#,D
io_topic = keypad/entry
debounce_cycles = 16

[compound:main_patch]
type = patch_map
pins = 1,2,3,4,5,A,B,C
io_topic = patch/main
scan_interval_ms = 100
```

## Validation Rules

PxIO validates configuration at startup.

Required checks:
- [global] and [mqtt] sections exist
- each gpio and compound entry has type and io_topic
- keypad_matrix has rows, cols, keys, and valid matrix dimensions
- patch_map has at least two pins
- qos is in range 0..2
- heartbeat_interval_ms is greater than 0

Validation outcomes:
- invalid required config: fatal startup error
- unsupported type on current platform: warning + continue

## Troubleshooting

Common startup failures:
- missing [mqtt] section
- invalid qos value
- malformed keypad keys matrix
- unresolved parent bus reference

Recommended diagnostics:
- set log_level = DEBUG
- verify broker connectivity with mosquitto_sub and mosquitto_pub
- confirm topic resolution behavior using one relative and one absolute io_topic

## Best Practices

- Keep one config file per runtime bridge instance.
- Use relative io_topic values unless cross-device sharing requires absolute topics.
- Use per-IO retain overrides only where startup state replay is required.
- Keep phase-2+ types in separate sections until runtime support is enabled.
