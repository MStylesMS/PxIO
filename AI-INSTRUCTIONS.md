# Pio — AI Instructions

Pio is the **GPIO-to-MQTT bridge** for Paradox (C++). It publishes inputs and accepts outputs over MQTT using the suite topic contract `{baseTopic}/{commands|events|state|warnings}`.

## Docs

Read `docs/` (especially MQTT_API / CONFIG / SPEC if present) before changing behaviour. Update docs in the same change as code.

## Conventions

- Conventional commits: `Docs:`, `Implement:`, `Fix:`, `Test:`, `Refactor:`, `Chore:`.
- Never break documented MQTT contracts without an approved doc update.
- Suite context: Paradox escape-room suite; sibling apps under `apps/`.
