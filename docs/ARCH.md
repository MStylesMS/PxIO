# PxIO Software Architecture

Version: 0.1.0-draft  
Last Updated: 2026-04-16

## Overview

PxIO is a GPIO-to-MQTT bridge targeting Raspberry Pi initially but designed from the start for portability to ESP32 and other Linux platforms.

The architecture is divided into two explicit layers:

- **Shared contract layer**: platform-independent schema, model types, command/event logic, and MQTT contract. This code can be shared with ESP32 and other variants with minimal or no changes.
- **Platform layer**: hardware-specific providers (GPIO, I2C) that implement the shared interfaces. Each target platform supplies its own provider implementations.

---

## Module Map

```
pxio/
├── src/
│   ├── shared/                  ← portable, no platform dependencies
│   │   ├── config/
│   │   │   ├── config_loader.h/cpp       INI file parser and validator
│   │   │   └── config_schema.h           typed config structs (GlobalConfig, MqttConfig, GpioEndpointConfig, etc.)
│   │   ├── model/
│   │   │   ├── endpoint.h                abstract Endpoint interface
│   │   │   ├── endpoint_registry.h/cpp   runtime registry of active endpoints
│   │   │   └── component_types.h         enums: EndpointType, Mode, LogLevel
│   │   ├── mqtt/
│   │   │   ├── mqtt_transport.h          pure-virtual MqttTransport interface
│   │   │   ├── paho_transport.h/cpp      Paho C async implementation of MqttTransport
│   │   │   └── mock_transport.h/cpp      mock implementation for unit tests
│   │   ├── command/
│   │   │   ├── command_dispatcher.h/cpp  parse, validate, route commands to endpoints
│   │   │   └── command_schema.h          command/event field constants and JSON helpers
│   │   ├── telemetry/
│   │   │   └── telemetry_publisher.h/cpp announce, heartbeat, state, warning publishers
│   │   └── log/
│   │       └── logger.h/cpp              logging facade (stdout/stderr + optional file sink)
│   │
│   └── platform/
│       └── linux_rpi/                   ← Raspberry Pi / Linux specific
│           ├── gpio/
│           │   ├── gpio_provider.h           pure-virtual GpioProvider interface (digital)
│           │   ├── analog_provider.h         pure-virtual AnalogProvider interface (phase 2: adc/dac/pwm)
│           │   ├── libgpiod_provider.h/cpp   libgpiod v2 implementation of GpioProvider
│           │   └── mock_gpio_provider.h/cpp  mock implementation for unit tests and dev
│           ├── i2c/
│           │   ├── i2c_bus.h/cpp             I2C bus open/close via /dev/i2c-N
│           │   └── mcp23017.h/cpp            MCP23017 expander driver
│           ├── endpoints/
│           │   ├── digital_in.h/cpp          digital_in endpoint (uses GpioProvider)
│           │   ├── digital_out.h/cpp         digital_out endpoint (uses GpioProvider)
│           │   ├── keypad_matrix.h/cpp       keypad_matrix compound endpoint
│           │   └── patch_map.h/cpp           patch_map compound endpoint
│           └── main_rpi.cpp                  Raspberry Pi entry point and startup sequence
│
└── tests/
    ├── unit/
    │   ├── test_config_loader.cpp
    │   ├── test_command_dispatcher.cpp
    │   ├── test_topic_resolution.cpp
    │   └── test_patch_map_canonicalize.cpp
    └── integration/
        └── test_mqtt_roundtrip.cpp           uses mock transport + mock GPIO provider
```

---

## Shared Contract Layer (platform-independent)

Everything under `src/shared/` has no dependency on Linux system calls, libgpiod, or hardware. It can be compiled on any C++20 toolchain.

### config/

Owns INI parsing and schema validation. Produces typed structs consumed by the rest of the system.

- `config_schema.h` defines `GlobalConfig`, `MqttConfig`, `GpioEndpointConfig`, `CompoundEndpointConfig`, `BusConfig`.
- `config_loader` reads the INI file, validates required fields, warns on unknown keys (never fatal), and populates the schema structs.
- Unknown keys: emit warning, skip key, continue loading.
- Invalid required keys: fatal at startup.

### model/

Defines the abstract `Endpoint` interface and the endpoint registry.

```cpp
class Endpoint {
public:
    virtual ~Endpoint() = default;
    virtual bool init() = 0;
    virtual bool read(std::string& value_out) = 0;
    virtual bool write(const std::string& value) = 0;
    virtual bool setMode(const std::string& mode) = 0;
    virtual std::string name() const = 0;
    virtual bool isAvailable() const = 0;
};
```

- Platform endpoint implementations (`DigitalIn`, `DigitalOut`, `KeypadMatrix`, `PatchMap`) inherit from `Endpoint`.
- `EndpointRegistry` maps string names (for example `gpio:led_red`) to `Endpoint*` instances.

### mqtt/

Owns the `MqttTransport` interface and its two implementations.

```cpp
class MqttTransport {
public:
    virtual ~MqttTransport() = default;
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool publish(const std::string& topic, const std::string& payload, int qos, bool retain) = 0;
    virtual bool subscribe(const std::string& topic, MessageCallback cb) = 0;
    virtual bool isConnected() const = 0;
};
```

- `PahoTransport` wraps the Eclipse Paho C async client.
- `MockTransport` records calls for test assertions with no network dependency.
- TLS support is not implemented in phase 1 but the interface is designed to accommodate it (additional `connectTLS()` variant) in a future phase.

### command/

Owns command envelope parsing, validation, routing, and event publishing.

Command lifecycle:
1. JSON received on `commands_topic`.
2. Parse envelope; if invalid JSON → publish `invalid_message_format` warning, stop.
3. Emit `command_received` event.
4. Look up target in `EndpointRegistry`; if not found → emit `command_failed` + `invalid_target`.
5. Dispatch to handler; emit `command_success`, `command_warning`, or `command_failed`.

Write-to-input edge case: emit `command_warning` with code `invalid_mode`, publish to `warnings_topic`, no state mutation.

Missing target cases:

| Situation | Event | Warning Code |
|---|---|---|
| Unknown target | command_failed | invalid_target |
| Provider unavailable | command_warning | endpoint_unavailable |
| Endpoint busy | command_warning | endpoint_busy |
| Write to input | command_warning | invalid_mode |

### telemetry/

Owns announce, heartbeat, state snapshot, and warning/error publishing. Reads `MqttConfig` for topic names and intervals. Independently invokes `MqttTransport`.

### log/

Thin logging facade wrapping `std::cerr`/`std::cout` with level filtering. Log levels: DEBUG, INFO, WARN, ERROR.

Sink policy:
- Default: stdout/stderr only (journald captures via systemd).
- Optional file sink: configured by `log_file` in `[global]` INI section or `--log-file` CLI flag.
- CLI overrides INI.
- File sink scope: WARN and ERROR messages only (major events and errors).

---

## Platform Layer (Raspberry Pi / Linux)

Everything under `src/platform/linux_rpi/` is Linux and Raspberry Pi specific.

### gpio/

`GpioProvider` is the hardware abstraction layer for digital pin-level operations.

```cpp
class GpioProvider {
public:
    virtual ~GpioProvider() = default;
    virtual bool openChip(const std::string& chip_path) = 0;
    virtual bool requestLine(int pin, PinDirection direction) = 0;
    virtual bool releaseLine(int pin) = 0;
    virtual bool setDirection(int pin, PinDirection direction) = 0;
    virtual bool setValue(int pin, int value) = 0;
    virtual int getValue(int pin) = 0;
};
```

- `LibgpiodProvider`: libgpiod v2 (Trixie baseline). Only provider compiled into production binaries.
- `MockGpioProvider`: configurable fake for unit and integration tests. Returns preset pin values. Records all calls for assertion.

#### Phase 2 boundary: AnalogProvider

`AnalogProvider` is the planned interface for scaled/continuous IO (adc, dac, pwm). It is defined in phase 1 as a stub header with no implementation so that phase 2 endpoint types can be added without restructuring the platform layer.

```cpp
// analog_provider.h — stub for phase 2 implementation
class AnalogProvider {
public:
    virtual ~AnalogProvider() = default;
    virtual bool readScaled(int channel, double& value_out) = 0;   // adc
    virtual bool writeScaled(int channel, double value) = 0;       // dac
    virtual bool setPwm(int channel, double duty_cycle, double frequency_hz) = 0;  // pwm
};
```

Phase 1 rule: no `AnalogProvider` implementation ships. The header exists to establish the interface contract and prevent future restructuring.

Phase 2 work: implement `LinuxAnalogProvider` (or kernel/device-specific variant) and `MockAnalogProvider` alongside their `GpioProvider` counterparts.

### i2c/

`I2cBus` opens and manages `/dev/i2c-N`. `Mcp23017` drives the MCP23017 16-bit I2C GPIO expander and implements `GpioProvider` against expander pins.

### endpoints/

Concrete endpoint implementations extending the shared `Endpoint` interface.

- `DigitalIn` / `DigitalOut`: use `GpioProvider` for raw pin access plus debounce logic.
- `KeypadMatrix`: row-scan compound component using `GpioProvider`. Decodes matrix to single-token key payload.
- `PatchMap`: multi-pin dynamic compound. Pins switch between input/output during scan. Output is canonicalized (numeric before alpha, ascending, uppercase).

Startup failure policy: if an endpoint fails to initialize, emit `endpoint_unavailable` warning, skip that endpoint, continue with remaining endpoints.

### main_rpi.cpp

Raspberry Pi entry point. Responsibilities:
1. Parse CLI arguments (including `--config` and `--log-file`).
2. Initialize logger.
3. Load and validate INI config.
4. Instantiate `LibgpiodProvider` (or `MockGpioProvider` in test mode).
5. Instantiate `PahoTransport`.
6. Build `EndpointRegistry` from config.
7. Initialize `CommandDispatcher` and `TelemetryPublisher`.
8. Connect MQTT transport.
9. Publish announce message.
10. Start main loop (poll_interval_ms, debounce, publish IO changes).
11. On SIGINT/SIGTERM: clean shutdown, disconnect, exit 0.

---

## Cross-Platform Portability Contract

To port to ESP32 or other Linux platforms:

| What to reuse | What to replace |
|---|---|
| `src/shared/` in its entirety | `src/platform/linux_rpi/` with a new platform directory |
| INI config schema and loader | Platform entry point |
| Command dispatcher | `GpioProvider` implementation for target hardware |
| Telemetry publisher | `MqttTransport` implementation if not Paho |
| Logger facade | Any platform-specific I2C drivers |
| Mock implementations for tests | |

Guidelines:
- No platform include or `#ifdef` permitted inside `src/shared/`.
- All hardware calls happen through `GpioProvider`, `AnalogProvider`, or `MqttTransport` interfaces only.
- Config schema structs must remain valid C++20 without RTTI or exception-dependent patterns to remain portable to bare-metal environments.
- `AnalogProvider` header ships in phase 1 as a stub; no implementation until phase 2.

---

## MQTT Contract Summary

All topics are resolved from INI config. IO topics use simple string payloads. Bridge control topics use JSON.

| Channel | Direction | Payload | Topic from INI |
|---|---|---|---|
| commands | inbound | JSON | commands_topic |
| state | outbound | JSON | state_topic |
| events | outbound | JSON | events_topic |
| warnings | outbound | JSON | warnings_topic |
| announce | outbound | JSON | announce_topic |
| heartbeat | outbound | JSON | heartbeat_topic |
| IO endpoints | outbound (in) | plain string | base_topic + io_topic |
| IO endpoints | inbound (out) | plain string | base_topic + io_topic |

IO topic resolution:
- If `io_topic` starts with `/`: use as absolute, strip leading `/`.
- Otherwise: `resolved = base_topic + "/" + io_topic`.

---

## Build Configuration

Two official release artifacts:

| Artifact | Toolchain | Target | OS |
|---|---|---|---|
| pxio-linux-arm64 | aarch64-linux-gnu | Pi 3/4/5, Zero 2 W (64-bit) | Trixie |
| pxio-linux-armv6hf | arm-linux-gnueabihf | Zero W and any Pi on 32-bit OS | Trixie |

CMake feature flags:
- `PXIO_GPIO_BACKEND`: `libgpiod` (default) or `mock`
- `PXIO_BUILD_TESTS`: `OFF` (default) or `ON`

Build host: Pi 5 running Trixie (native arm64 build + cross-compile armv6hf via installed toolchain).

---

## Recommended AI Model for Implementation

**Claude Sonnet 4.5 or Claude Opus 4** via GitHub Copilot (or direct API).

Reasoning:
- This codebase involves non-trivial C++20 interface design, a two-layer abstraction strategy, and precise MQTT contract conformance. It benefits from a model that holds the full architecture context across multiple related files at once.
- Sonnet 4.5 handles multi-file C++ generation accurately and follows interface contracts well for a project of this size and complexity.
- Opus 4 is appropriate if you want the model to reason more deeply about edge cases (for example patch_map canonicalization, command lifecycle correctness, and cross-platform portability constraints) before generating code.
- GPT-4o is a viable alternative but tends to produce more variation in C++ idiom consistency across files.

Practical recommendation:
- Use **Sonnet 4.5** for routine module generation (config loader, transport wrapper, individual endpoints).
- Use **Opus 4** for the command dispatcher, telemetry publisher, and any logic where correctness under edge cases matters more than speed of generation.
