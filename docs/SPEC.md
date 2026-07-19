# PxIO (Paradox IO) Specification

Version: 0.2.0-draft  
Last Updated: 2026-04-16

## Overview

PxIO is a GPIO-to-MQTT bridge focused on Raspberry Pi in the initial release.

Primary objectives:
- Bridge physical IO to MQTT topics using a deterministic contract.
- Preserve lightweight IO payloads for low-latency and compatibility.
- Support local GPIO and bus-attached IO providers.
- Align topic and operational patterns with existing Paradox products.

## Product Scope

In scope:
- Headless bridge runtime
- INI-driven configuration
- MQTT command and telemetry interface
- Primitive and compound IO component model

Out of scope for phase 1:
- Embedded full SPA web server in bridge process
- System tray UI tooling
- Direct I2S audio control

## System Architecture

Core modules:
1. Config loader and validator
2. MQTT transport and dispatcher
3. IO provider layer
4. Component engine
5. Telemetry publisher

### Config Loader and Validator

- Reads a single INI file per bridge instance.
- Supports default path and CLI override.
- Enforces schema and required fields.

### MQTT Transport and Dispatcher

- Subscribes to commands_topic.
- Publishes state, events, warnings, announce, and heartbeat payloads.
- Applies configurable QoS and retain policies.

### IO Provider Layer

- Raspberry Pi GPIO provider.
- I2C provider and expander support for parity with gpio-cpp behavior.
- Extension points for SPI, RS-485, and other buses.

### Component Engine

- Primitive endpoints: digital_in, digital_out.
- Compound endpoints: keypad_matrix, patch_map.
- Runtime mode-switch support for compatible endpoints.

### Telemetry Publisher

- announce_topic: one online message per startup.
- heartbeat_topic: periodic health snapshots.
- warnings_topic: degraded operation and unsupported type notices.

## Configuration Model

INI section naming convention:
- [global]
- [mqtt]
- [gpio:<name>]
- [compound:<name>]
- [bus:<name>]

Nested section names use colon separators, not dot separators.

File model:
- One config file per instance.
- Default file name: pxio.ini in current directory.
- Override supported: pxio --config <path>.

## MQTT Contract Requirements

### Topic Model

- bridge commands: commands_topic
- bridge telemetry: state_topic, events_topic, warnings_topic
- lifecycle channels: announce_topic and heartbeat_topic
- IO channels: resolved per endpoint using base_topic + io_topic rules

### Topic Resolution Rules

1. If io_topic starts with /, treat as absolute and ignore base_topic.
2. Otherwise, join base_topic and io_topic with /.

Examples:
- base_topic paradox/spycatcher + io_topic keypad = paradox/spycatcher/keypad
- base_topic paradox/spycatcher + io_topic /paradox/keypad = paradox/keypad

### Payload Rules

- IO data payloads use simple strings whenever possible.
- Bridge commands use JSON.
- Bridge state/events/warnings use JSON.
- Event subtype detail is represented by JSON fields, not nested event topics.

### Command and Event Schema (Normative)

Command envelope fields:
- command: required command name
- target: optional unless required by command type
- request_id: optional caller correlation id
- additional command-specific fields (for example value, mode)

Command outcome events are published to events_topic with:
- timestamp: ISO 8601 timestamp
- type: command
- event: command_received, command_success, command_failed, or command_warning
- command: command name
- target: optional target
- request_id: optional caller correlation id
- message: human-readable status
- details: optional structured object

Warnings/errors are published to warnings_topic with:
- timestamp
- level: info, warning, error, fatal
- code: machine-readable warning/error code
- message: human-readable summary
- target: optional endpoint/component
- details: optional structured object

### QoS and Retain Rules

- default QoS is 1, configurable in INI.
- global_retain default is false, configurable in INI.
- per-IO retain override is supported and takes precedence.

## Component Specifications

### Primitive Types

Phase 1:
- digital_in
- digital_out

Phase 2+ additions:
- adc
- dac
- pwm
- additional primitive types

### keypad_matrix

Requirements:
- explicit rows and cols configuration
- matrix key mapping provided in INI
- publishes one decoded token per event to IO topic

Payload examples:
- 1
- A
- #

### patch_map

Requirements:
- supports dynamic pin mode switching during scans
- supports chains longer than two members
- outputs canonical chain representation

Canonicalization rules:
1. sort lowest to highest
2. numeric ids before alphabetic ids
3. alphabetic ids uppercase
4. lowercase input is tolerated but normalized to uppercase

Examples:
- canonical: 1-3-5
- canonical: 2-A-C
- non-canonical: 3-5-1

## Platform Strategy

- Linux/Raspberry Pi bridge and ESP32 bridge are separate codebases.
- Shared contract must remain aligned where practical:
  - INI syntax conventions
  - MQTT schema and topic behavior
- Unsupported types must not terminate runtime:
  - publish warning
  - log warning
  - continue operation

## Phase Roadmap

### Phase 1: Bridge Core (Raspberry Pi parity)

Deliverables:
- INI configuration and strict validation
- MQTT command, state, events, warnings, announce, heartbeat channels
- digital_in and digital_out
- keypad_matrix and patch_map
- Raspberry Pi GPIO provider
- I2C expander support for gpio-cpp baseline parity
- Nginx-served basic HTML debug page (outside bridge process)

Success criteria:
- Covers baseline gpio-cpp Raspberry Pi functionality.
- Maintains stable operation under invalid optional type declarations.

### Phase 2: Extended IO and Bus Expansion

Deliverables:
- adc, dac, pwm and related primitives
- ws2812 initial support
- RS-485 early integration model

### Phase 3: Operator Experience

Deliverables:
- optional tray application
- bridge status monitor
- start/stop/restart controls
- config editing workflow
- expanded HTML UX

### Phase 4: Advanced Expansion

Deliverables:
- expanded LED family support (ws2811/ws2815/ws2818/apa102/sk9822)
- additional bus and component types
- deeper Linux and ESP32 contract convergence

## Operational Requirements

- Runtime must remain resilient to unsupported configured types.
- Warning telemetry must be published for degraded conditions.
- Heartbeat cadence must be configurable in INI.
- Announce and heartbeat topics must be independently configurable.

## Open Technical Items

- Finalize patch scan cadence and stability thresholds.
- Define phase-2 numeric scaling/calibration metadata for adc/dac/pwm.
