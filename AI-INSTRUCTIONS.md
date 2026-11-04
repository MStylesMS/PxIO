# PxIO — AI Instructions

## Required: keep this application generic

This requirement outranks every other instruction in this file.

Do not put room-specific behavior or information into this application. Room names, show copy, cues, timings, asset filenames, topics, and hardware layouts belong in that room's config and media, not in this repo.

Do not edit this application's code while working on a room, a game config, or any other project that uses this app, unless the person you are working with has explicitly authorized a change to this application in the current request. A bug found during a room test is not authorization. If the room cannot get the behavior from the existing command and config contract, stop and ask before editing this repo.

An authorized change still has to stay generic. It must be correct for every room that uses this app.

PxIO is the **GPIO-to-MQTT bridge** for Paradox (C++). It publishes inputs and accepts outputs over MQTT using the suite topic contract `{baseTopic}/{commands|events|state|warnings}`.

## Docs

Read `docs/` (especially MQTT_API / CONFIG / SPEC if present) before changing behaviour. Update docs in the same change as code.

## Conventions

- Conventional commits: `Docs:`, `Implement:`, `Fix:`, `Test:`, `Refactor:`, `Chore:`.
- Never break documented MQTT contracts without an approved doc update.
- Suite context: Paradox escape-room suite; sibling apps under `apps/`.

## Suite standards

Public suite brief + contracts live in [../PxH/docs/standards/](../PxH/docs/standards/) (folder, not a single file) — especially `AI-INSTRUCTIONS.md` and `MQTT-CONTRACT.md`. Read those before changing MQTT topics or shared conventions. If you change a standard, update the file under PxH `docs/standards/` first and propagate to other repos' docs in the same work.

If the workspace has `Px-Suite/` (or `/opt/paradox/Px-Suite`), use it for internal notes, cross-cutting pending plans, and business overview — do not put those into distributed PxH standards.

