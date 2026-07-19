# PxIO

PxIO (Paradox IO) is a GPIO-to-MQTT bridge focused on Raspberry Pi devices.

## What It Does

- Maps GPIO and bus-connected IO to MQTT topics
- Supports simple string payloads for IO values
- Supports JSON commands/events/state/warnings for bridge control
- Uses INI configuration with colon section naming (for example, [gpio:led_red])

## Documentation

- docs/MQTT_API.md
- docs/CONFIG_INI.md
- docs/SPEC.md

## Paradox Family

PxIO is one of seven Paradox products. All use the shared MQTT topic convention `{baseTopic}/{commands|events|state|warnings}`.

- **PFx** — media/audio/lights/relays controller (Node.js)
- **PxO** — game orchestration engine (Node.js, EDN)
- **PxC** — configurable clock app framework (React build)
- **PxT** — player terminal kiosk (Electron)
- **PxIO** — this project, GPIO-to-MQTT bridge (C++)
- **PxB** — Z-Wave / Zigbee / Thread to MQTT bridge (Node.js)
- Rooms: `agent22`, `houdinis-challenge` — game packages consumed by PxO + PFx

## Build (Current)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

## Project Status

This repository is in active design and early implementation planning.


## License

Dual-licensed:

- **AGPL-3.0** for open source use — see [LICENSE](LICENSE).
- **Commercial license required** for proprietary or revenue-generating use that does not comply with AGPL-3.0 — see [COMMERCIAL.md](COMMERCIAL.md).

Copyright © 2026 Mark Stevens.
