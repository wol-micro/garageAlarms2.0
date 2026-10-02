# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

garageAlarms 2.0 — a hubless alarm network: ESP32-S3 + ESP32-H2 "logic" nodes and ESP32-H2
"collector" nodes over a Zigbee mesh, alerts to Telegram, Pushover and MQTT. It replaces the
single-box firmware in `Lindenson/garageAlarms` (1.x), which stays in service until epic 5.

Ordering of concerns, in every decision: **never lose an event > never flood the user >
everything else.** Duplicates are acceptable; losses are not.

## Source of truth

Implementation follows the planning documents; read them before changing behaviour:

| Document | Path |
| --- | --- |
| Specification (13 capabilities) + companions | `_bmad-output/specs/spec-garage-alarms-2/SPEC.md` |
| Architecture spine (AD-1…AD-25, binding) | `_bmad-output/planning-artifacts/architecture/architecture-garageAlarms-2026-09-29/ARCHITECTURE-SPINE.md` |
| UX contracts | `_bmad-output/planning-artifacts/ux-designs/ux-garageAlarms-2026-09-30/DESIGN.md`, `EXPERIENCE.md` |
| Epics and stories (with UX-DR) | `_bmad-output/planning-artifacts/epics.md` |
| Start values of all tunables | `_bmad-output/specs/spec-garage-alarms-2/tunables.md` |

A change that contradicts an AD is a conflict to raise, not a local decision. Planning
documents are updated through their `.memlog.md` (BMad skills), not by hand.

Planning documents are in Russian; code, comments and logs are English; user-facing text is
Russian.

## Build

Toolchain: **ESP-IDF v6.0.3** installed at `~/esp/v6.0.3/esp-idf` (targets esp32s3, esp32h2).
An older `~/Esp32/esp-idf` (v5.5-dev) exists for other projects — do not use it here.

```bash
. ~/esp/v6.0.3/esp-idf/export.sh
```

Project layout and per-app build commands are created by story 1.1; update this section then.
