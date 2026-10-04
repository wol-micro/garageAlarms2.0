# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

garageAlarms 2.0 — a hubless alarm network: ESP32-S3 + ESP32-H2 "logic" nodes and ESP32-H2
"collector" nodes over a Zigbee mesh, alerts to Telegram, Pushover and MQTT. It replaces the
single-box firmware in `wol-micro/garageAlarms` (1.x), which stays in service until epic 6.

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

Layout:

- `components/` — shared ESP-IDF components (`snake_case`): `ga_config` (every constant, AD-25),
  `proto`, and the actors `mesh delivery membership storage sensors sys` (both firmwares) plus
  `config rules notify ui` (logic node only).
- `apps/logic-s3` (esp32s3) and `apps/collector-h2` (esp32h2) pull `components/` through
  `EXTRA_COMPONENT_DIRS` and list their components in `COMPONENTS`; the collector must never list
  the logic-only ones.

```bash
idf.py -C apps/logic-s3 set-target esp32s3 build          # set-target once per clean build dir
idf.py -C apps/logic-s3 -p <port> flash monitor
idf.py -C apps/collector-h2 set-target esp32h2 build
idf.py -C apps/collector-h2 -p <port> flash monitor
gcc -fsyntax-only -x c components/ga_config/include/ga_config.h   # ga_config.h stays IDF-free
```

Component shape (AD-27): every actor component has `core/` (pure deterministic C: no FreeRTOS,
no ESP-IDF headers, no clock reads; only `ga_config.h` and `proto` allowed), `port/` (interfaces
to external dependencies), `shell/` (the FreeRTOS task — the only place for RTOS code and
core/priority binding from the `GA_TASK_*` map) and `test/`. `esp_err_t` only in `shell/` and
`port/`. No timeout, interval or size literals outside `ga_config.h` (AD-25).
