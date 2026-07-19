# PxIO Compile + Implementation Checklist

Version: 0.1.0-draft  
Last Updated: 2026-04-16

This checklist captures the 22 decisions/questions we need to finalize before and during C++ implementation.

Status values:
- Answered
- Open
- Needs clarification

| # | Question | Current Answer / Notes | Status |
|---|---|---|---|
| 1 | Is phase-1 scope locked to digital_in, digital_out, keypad_matrix, patch_map, and baseline I2C expander parity? | Yes. | Answered |
| 2 | Confirm we should use libgpiod instead of legacy sysfs GPIO for Raspberry Pi support. | Yes ("Correct"). | Answered |
| 3 | Is TLS required in phase 1 for MQTT transport? | No. | Answered |
| 4 | GPIO backend option: should we use only libgpiod v2 API, or support both v1/v2 at compile-time? | libgpiod v2 only. Baseline compile target is Raspberry Pi OS Trixie. | Answered |
| 5 | GPIO backend option: should there be a mock/simulated GPIO backend for desktop/unit tests? | Yes. Add mock GPIO backend for tests/dev workflows. | Answered |
| 6 | GPIO backend option: should unsupported platform/provider configurations warn-and-continue at runtime? | Warn and skip endpoint (bridge continues for healthy endpoints). | Answered |
| 7 | MQTT stack option: use Eclipse Paho C async client or Mosquitto client library? | Eclipse Paho C (async client model). | Answered |
| 8 | MQTT stack option: should we abstract transport behind a provider interface to swap MQTT libs later? | Yes. Use internal transport interface layer. | Answered |
| 9 | Clarify TLS question (what TLS means for MQTT in this project). | TLS = encrypted MQTT transport; phase 1 decision is no TLS requirement. Keep config extensible for later TLS enablement. | Answered |
| 10 | Clarify command semantics and edge-case policy scope before coding starts. | Command lifecycle: command_received emitted for every valid command envelope; command_failed for schema/validation errors; command_warning for degraded but non-fatal execution; command_success only when operation fully completes. Invalid JSON publishes warning_code invalid_message_format and does not emit command_received. | Answered |
| 11 | Unknown INI keys policy: fatal or warn-and-continue? | Warn and continue; also report unknown keys. | Answered |
| 12 | Retain/QoS precedence policy: exact override order for global vs endpoint-level settings. | Precedence: endpoint-level setting > global [mqtt] setting > spec hard default. Phase 1: per-endpoint retain override supported; QoS remains global-only for phase 1. | Answered |
| 13 | Write-to-input behavior: should write fail hard, soft-fail warning, or auto-mode-switch? | Soft warning only: emit command_warning, publish to warnings_topic, no state mutation, no auto mode-switch. | Answered |
| 14 | If write fails, how should it be surfaced? | Warning only; write warning to logs and publish to MQTT warnings topic. | Answered |
| 15 | Should set_mode be allowed for compound components? | Allowed in code-path, with guardrails. | Answered (implementation details pending) |
| 16 | Missing/invalid target behavior at runtime (unknown target, unavailable endpoint, busy endpoint). | Unknown target: command_failed + warning code invalid_target. Provider unavailable: command_warning + warning code endpoint_unavailable. Busy/transient failure: command_warning + warning code endpoint_busy. Runtime continues in all three cases. | Answered |
| 17 | Runtime/service packaging now: systemd unit in phase 1 or defer? | Yes. Include systemd service unit in phase 1. Pi deployments are headless and require persistent service management and auto-restart. | Answered |
| 18 | Logging sink now: stdout only, stdout+journald, or file logging as first-class option? | Default: stdout/stderr only (journald captures via systemd). Optional log file output configurable via INI key log_file or CLI flag --log-file. CLI overrides INI. If neither specified, stdout/stderr only. Log file scope: major events and errors only. | Answered |
| 19 | Hardware validation matrix: minimum boards physically available for acceptance testing. | Pi 5 (arm64, Trixie); Pi 4 (arm64, Trixie); Pi 3 (arm64 and armv6hf, Trixie); Zero W (armv6hf, 32-bit, Trixie); Zero 2 W (arm64, Trixie). | Answered |
| 20 | Release artifact policy: do we publish one arm64 and one armv6hf build as baseline artifacts? | Yes. Two binaries: pxio-linux-arm64 and pxio-linux-armv6hf. | Answered |
| 21 | Build host strategy: are we building on oldest supported distro/sysroot for forward compatibility? | Trixie is the oldest and only supported release. Build and ship binaries targeting Trixie glibc and libgpiod v2. No prior release sysroot required. | Answered |
| 22 | CI/release matrix policy: which Pi OS releases are officially supported/tested (for example prior release + Trixie)? | Trixie only. Prior release (Bookworm) is not officially supported. | Answered |

## Working Notes

All checklist items are resolved. No open decisions remain.

### Command lifecycle reference (item 10)

- Valid command envelope received: emit command_received
- Invalid JSON or missing required fields: publish warning_code invalid_message_format; do not emit command_received
- Operation failed hard: emit command_failed
- Operation completed with degradation: emit command_warning (may be followed by command_success)
- Operation fully completed: emit command_success

### Retain/QoS precedence reference (item 12)

1. Endpoint-level retain setting (if present)
2. Global [mqtt] retain/QoS setting
3. Spec hard default (retain = false, qos = 1)

Phase 1: per-endpoint QoS override is not implemented; QoS is global only.

### Warning codes reference (item 16)

| Situation | Event | Warning Code |
|---|---|---|
| Unknown target | command_failed | invalid_target |
| Provider unavailable | command_warning | endpoint_unavailable |
| Endpoint busy / transient failure | command_warning | endpoint_busy |
| Write to input endpoint | command_warning | invalid_mode |
| Invalid JSON command | (none) | invalid_message_format |

### Log file option (item 18)

INI key (under [global]):
```ini
log_file = /var/log/pxio/pxio.log
```

CLI flag:
```
pxio --log-file /var/log/pxio/pxio.log
```

CLI value takes precedence over INI value. If neither is set, only stdout/stderr are used.

## Raspberry Pi Hardware + Binary Matrix

### Zero 2 W clarification

Yes, Raspberry Pi Zero 2 W exists.

- CPU: Cortex-A53 (ARMv8)
- Supports 64-bit OS and 64-bit binaries
- Also supports 32-bit OS and 32-bit binaries

Original Zero W remains ARMv6-class and should be treated as 32-bit only for practical support.

### Confirmed release artifacts

Two binaries for all supported hardware:

| Artifact | Build Target | Build On | Runs On Models | OS Bitness | Distro |
|---|---|---|---|---|---|
| pxio-linux-arm64 | aarch64-linux-gnu | Trixie | Pi 3, Pi 4, Pi 5, Zero 2 W | 64-bit | Trixie |
| pxio-linux-armv6hf | arm-linux-gnueabihf (armv6 baseline) | Trixie | Zero W, Pi 3, Pi 4, Pi 5, Zero 2 W (when running 32-bit OS) | 32-bit | Trixie |

### Build strategy

1. Build both artifacts on a Pi 5 (or equivalent aarch64 host) running Trixie.
2. Cross-compile armv6hf artifact using arm-linux-gnueabihf toolchain installed on the Trixie host.
3. Trixie is the minimum and only supported OS release.
4. No prior release (Bookworm) sysroots or compatibility shims needed.
