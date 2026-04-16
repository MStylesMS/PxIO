# PIo

PIo (Paradox IO) is a GPIO-to-MQTT bridge focused on Raspberry Pi devices.

## What It Does

- Maps GPIO and bus-connected IO to MQTT topics
- Supports simple string payloads for IO values
- Supports JSON commands/events/state/warnings for bridge control
- Uses INI configuration with colon section naming (for example, [gpio:led_red])

## Documentation

- docs/MQTT_API.md
- docs/CONFIG_INI.md
- docs/SPEC.md

## Build (Current)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

## Project Status

This repository is in active design and early implementation planning.
