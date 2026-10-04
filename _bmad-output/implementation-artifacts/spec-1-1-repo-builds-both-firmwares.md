---
title: 'Story 1.1: garageAlarms2.0 repository builds both firmwares'
type: 'chore'
created: '2026-10-02'
status: 'done'
baseline_commit: 'a6238523659085b0a6bc62b633013be013aac10b'
route: 'full'
route_source: 'auto'
review: 'thorough'
review_source: 'auto'
lenses_ran: ['blind-hunter', 'edge-case-hunter', 'verification-gap', 'intent-alignment']
review_loop_iteration: 0
context:
  - '{project-root}/_bmad-output/implementation-artifacts/epic-1-context.md'
  - '{project-root}/_bmad-output/specs/spec-garage-alarms-2/tunables.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** The repository holds only planning documents; there is no ESP-IDF project, so no later story has a working build to start from.

**Approach:** Create the monorepo skeleton from the architecture Structural Seed: shared `components/` (actor stubs already in the AD-27 shape `core/ port/ shell/ test/`, plus a real `ga_config.h` with all `tunables.md` constants and the AD-26 task map) and two apps, `apps/logic-s3` (esp32s3) and `apps/collector-h2` (esp32h2), that build on ESP-IDF v6.0.3 and log an identity banner at boot.

## Boundaries & Constraints

**Always:**
- ESP-IDF v6.0.3 from `~/esp/v6.0.3/esp-idf`; never `~/Esp32/esp-idf`.
- Pin exactly `espressif/esp-zigbee-lib: "2.0.4"` and `espressif/cbor: "7.0.0"` (no `~`/`^`).
- Every tunable in `ga_config.h` uses exactly the `GA_*` name from `tunables.md`; all durations are in milliseconds and sizes in bytes (stated once in the header and per macro in a comment); soft-overridable ones are marked `/* default, soft-overridable */`. `GA_CONFIG_VERSION` is an integer starting at 1.
- `ga_config.h` is plain C with no IDF/FreeRTOS includes, so `core/` code and host tests can use it (AD-27).
- Component names `snake_case`; code, comments, logs English; `ESP_LOG*` with a component tag.
- Both apps share `components/` via `EXTRA_COMPONENT_DIRS`; the collector build must not pull logic-only components (`config`, `rules`, `notify`, `ui`).

**Never:**
- No actor runtime, Zigbee init, WiFi, FRAM or proto encoding — later stories own those. Stubs hold only the directory shape, a public header and, if IDF needs it, one placeholder source.
- No Arduino code or ports from 1.x.
- No literal timeout/size values outside `ga_config.h` (AD-25).

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Logic build | `idf.py -C apps/logic-s3 set-target esp32s3 build` on clean clone | Build succeeds, deps resolved to pinned versions | — |
| Collector build | `idf.py -C apps/collector-h2 set-target esp32h2 build` | Build succeeds | — |
| Task map | `ga_config.h` | `GA_TASK_<ACTOR>_CORE/_PRIO/_STACK` for every actor; S3 core 0 = `Notify`, `Ui`, `Sys`; core 1 = `Mesh` (+ Zigbee stack task), `Delivery`, `Storage`, `Membership`, `Sensor`, `Rules`, `Config`; priorities strictly in AD-26 order, all below lwIP (18) | Collector build ignores the core field (single core) |
| Boot banner | Flashed node powers up | One log line: role (`logic`/`collector`), EUI-64 (16 hex), firmware version (`esp_app_get_description()->version`), `GA_CONFIG_VERSION` | Logic node: EUI-64 is provisional (see Decisions) |

**Decisions:**
- EUI-64 on the logic node (human, 2026-10-02): log a provisional ID derived from the S3 base MAC, explicitly marked `provisional` in the banner (e.g. `eui64=…(provisional)`). It is a log value only — never stored, sent or used as a node ID. Story 1.3 must additionally log the real EUI-64 once the H2 RCP link is up. The collector logs its own 802.15.4 EUI-64 with no marker.

</frozen-after-approval>

## Code Map

Greenfield; nothing to reuse except:
- `.gitignore` -- already ignores `build/`, `managed_components/`, `sdkconfig`, `dependencies.lock`; keep. Decide nothing about `dependencies.lock` (stays ignored).
- `README.md` -- showcase README (sections Why…License, 302 lines); add a `## Build` section before `## Roadmap`; do not rewrite other sections.
- `CLAUDE.md` -- `## Build` section says "updated by story 1.1"; replace its last paragraph with real commands.
- `_bmad-output/specs/spec-garage-alarms-2/tunables.md` -- source of start values. Ranges resolved from 1.x `config.h`: smoke/alarm debounce 200 ms, cooldown 2 min; motion debounce 400 ms, cooldown 5 min, PIR warm-up 60 s.
- `esp_read_mac(…, ESP_MAC_IEEE802154)` -- returns 8-byte EUI-64 on esp32h2 only (`SOC_IEEE802154_SUPPORTED`); esp32s3 has no 802.15.4, its Zigbee EUI-64 belongs to the H2 RCP (AD-7).
- Registry check: `esp-zigbee-lib` 2.0.x has no `esp-zboss-lib` dependency (unlike the 1.6 examples in IDF); do not add zboss-lib.

## Tasks & Acceptance

**Execution:**
- [x] `components/ga_config/{CMakeLists.txt,include/ga_config.h}` -- header-only component with `GA_CONFIG_VERSION` and every `tunables.md` row as macros, plus `GA_ZB_FRAME_MAX_BYTES 80` (NFR5) -- single source of constants (AD-25).
- [x] `components/{mesh,delivery,membership,storage,sensors,sys,config,rules,notify,ui}/` -- actor stubs: `core/`, `port/`, `shell/`, `test/` (empty dirs keep a `.gitkeep`), `CMakeLists.txt` (`idf_component_register`), `include/<name>.h` with a one-line purpose comment and the owning ADs -- AD-27 shape from day one.
- [x] `components/proto/` -- not an actor: `src/`, `include/proto.h`, `test/`; plain C, no IDF includes (AD-27) -- story 1.2 fills it.
- [x] `ga_config.h` task map -- `GA_TASK_*` start values: priorities Mesh 12 > Delivery 11 > Storage 10 > Membership = Sensor 9 > Rules 8 > Config 7 > Notify 6 > Ui 5 > Sys 4; stacks 4096 B, Notify/Ui/Zigbee stack task 8192 B; cores per AD-26 -- tuned in story 2.1.
- [x] `components/proto/idf_component.yml` -- pin `espressif/cbor: "7.0.0"` -- proto owns CBOR (AD-5).
- [x] `components/mesh/idf_component.yml` -- pin `espressif/esp-zigbee-lib: "2.0.4"` -- mesh owns Zigbee (AD-12).
- [x] `components/sys/{sys_banner.c,include/sys.h}` -- `sys_log_banner(const char *role)`: logs role, EUI-64 (H2: `ESP_MAC_IEEE802154`; S3: EUI-64 built from base MAC with `FFFE` inserted, tagged `(provisional)`), app version, `GA_CONFIG_VERSION` -- shared by both apps; the provisional value is local to this function.
- [x] `apps/logic-s3/{CMakeLists.txt,main/CMakeLists.txt,main/main.c,sdkconfig.defaults}` -- project using `EXTRA_COMPONENT_DIRS ../../components`; `app_main` calls `sys_log_banner("logic")`; `sdkconfig.defaults`: target esp32s3, CPU 240 MHz, flash 16 MB, PSRAM on in Octal mode, task watchdog checking idle tasks on both cores, `CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0=y`.
- [x] `apps/collector-h2/…` -- same shape, `COMPONENTS` limited to the shared set; role `collector`; target esp32h2, flash 4 MB (Waveshare MINI-1-N4).
- [x] `README.md`, `CLAUDE.md` -- Build section: export, set-target, build, flash, monitor for both chips; CLAUDE.md also states the component shape rule (`core/` pure C, RTOS only in `shell/`, AD-27).

**Acceptance Criteria:**
- Given a clean clone, when both apps are built, then each succeeds and `dependencies.lock` shows esp-zigbee-lib 2.0.4 and cbor 7.0.0.
- Given the collector build, when its component list is inspected, then `config`, `rules`, `notify`, `ui` are absent.
- Given `ga_config.h`, when compared with `tunables.md`, then every row has a macro with the same `GA_*` name and start value.
- Given any actor component, when its folder is listed, then it has `core/`, `port/`, `shell/`, `test/`.
- Given `ga_config.h`, when compiled with plain host `gcc -fsyntax-only`, then it compiles (no IDF dependency).
- Given `apps/logic-s3` built, when `sdkconfig` is inspected, then Octal PSRAM, 240 MHz, 16 MB flash, TWDT on both cores and lwIP affinity CPU0 are set.

## Implementation Notes

- Both apps build with exit 0 on ESP-IDF v6.0.3; `dependencies.lock` resolves esp-zigbee-lib 2.0.4 and cbor 7.0.0.
- Calls where the spec left room: Zigbee stack task is `GA_TASK_ZIGBEE_*` (core 1, priority = Mesh); `GA_DAILY_REPORT_TIME` is ms after local midnight; `GA_STATS_DAYS` and `GA_OUTBOX_CRIT_QUOTA` are dimensionless (days, percent); `GA_WEB_REFRESH` takes 5 s (low end of 5–10 s); `GA_FRAM_*` region names and `GA_TASK_LWIP_PRIO` added beyond `tunables.md`; the `config` stub's guard is `GA_CONFIG_ACTOR_H` because `GA_CONFIG_H` belongs to `ga_config.h`.
- Review patches (pass 1): Zigbee enabled in both `sdkconfig.defaults` and linked through `mesh`; collector build guard against logic-only components; banner moved to `sys/shell/`; 32-bit `U` suffixes; lwIP priority assert; extra static asserts; WiFi core 0 explicit; doc fixes.
- `fw=` shows a commit hash (possibly `-dirty`) until the repo has a tag.

## Spec Change Log

## Review Triage Log

**Pass 1 (2026-10-04)** — lenses: blind-hunter, edge-case-hunter, verification-gap, intent-alignment. Verdicts: medium 7, low 13, false 2. No intent_gap or bad_spec, so no loopback.

| # | Finding | Verdict | Route | Evidence / action |
|---|---------|---------|-------|-------------------|
| 1 | esp-zigbee-lib only resolved, never compiled or linked (intent-alignment e) | medium | patch | Without `CONFIG_ZB_ENABLED` the lib registers no sources. Enabled ZB (ZCZR; S3 Spinel UART, H2 native) in both `sdkconfig.defaults`; `mesh` PRIV_REQUIRES the lib; both maps list `libesp-zigbee*.a` (remote on S3, native on H2), link clean. API use is proven in 1.3 |
| 2 | Collector `COMPONENTS` does not block transitive logic-only deps (verification-gap, edge, blind) | medium | patch | IDF adds REQUIRES transitively. Added a BUILD_COMPONENTS guard with FATAL_ERROR; negative test (storage PRIV_REQUIRES config) fails the build as expected |
| 3 | `sys_banner.c` at component root breaks the AD-27 rule added in the same change (blind, verification-gap, intent f) | medium | patch | Moved to `components/sys/shell/` |
| 4 | Durations `UL`: 32-bit on target, 64-bit on host; TTL > INT32_MAX (edge, blind) | medium | patch | Numeric suffixes `U` (32-bit on both); header documents uint32 ms, no signed storage; assert that `GA_TOMBSTONE_TTL` did not wrap |
| 5 | `GA_TASK_LWIP_PRIO` hand copy never checked against sdkconfig (edge, blind) | medium | patch | Static assert vs `CONFIG_LWIP_TCPIP_TASK_PRIO` in `sys/shell/sys_banner.c` |
| 6 | Lease renew < TTL not asserted (edge, blind) | low | patch | Added; also uplink lost < restored, web refresh 5–10 s, daily report < 24 h |
| 7 | Zigbee priority outside the ordering chain; cores not range-checked (blind) | low | patch | Asserts ZIGBEE_PRIO == MESH_PRIO and every `_CORE` ≤ 1 |
| 8 | WiFi core-0 pinning implicit (blind) | low | patch | Default is already core 0; made explicit in the S3 defaults |
| 9 | FRAM superblock comment contradicts itself (blind) | low | patch | Reworded |
| 10 | Serial ports hard-coded in docs (blind) | low | patch | `-p <port>` with a note |
| 11 | README documents table still says 67 stories (blind) | low | patch | 68 |
| 12 | CLAUDE.md source-of-truth table still says AD-1…AD-25 (blind) | low | defer | Agent-context file; deferred per rules and flagged to the human |
| 13 | TWDT only logs (`CONFIG_ESP_TASK_WDT_PANIC` n), so a hung node never resets (edge) | medium | defer | Reset-or-log on hang is an AD-3 policy choice; belongs with actor TWDT registration in 2.1 |
| 14 | No partition table: no OTA slots, no `zb_storage`/`zb_fct` (blind) | medium | defer | Needed by 1.3 (Zigbee NVS) and 6.2 (OTA); must land before production boards are flashed |
| 15 | Provisional EUI-64 derivation untested (verification-gap) | low | defer | Lens disposition defer; move MAC→EUI into `sys/core/` with a host test once 1.2's runner exists |
| 16 | `role` NULL not guarded (edge, blind) | low | reject | Both callers pass literals; unlikely, and the fix adds a guard |
| 17 | MAC read failure prints a normal-looking EUI (edge, blind) | low | reject | A warning is already logged; unlikely, and the fix adds a branch |
| 18 | `test/host/` from the seed and a core-only host target are missing (intent a, blind) | low | reject | Owned by story 1.2 (host test runner, coverage) |
| 19 | AD-27 shape not enforced by any check (intent d) | low | reject | Enforcement is 1.2's core-only build target |
| 20 | Banner not verified on hardware (intent b) | low | reject | Already a recorded manual check; no boards attached |
| 21 | Diff omits the spec and sprint-status changes (intent h, blind) | false | reject | Bookkeeping files deliberately kept out of the code diff |
| 22 | README AD-count change is outside the approach (intent g) | false | reject | Corrects a count made stale by the 2026-10-04 spine update; no harm |

## Design Notes

The verify-tech review flagged that esp-zigbee-lib 2.0.4 was released against IDF v5.5.4; this story's build is the first proof it resolves and links on v6.0.3 — if it fails, record the error and stop rather than switching IDF.

Version shown in the banner comes from `PROJECT_VER` (IDF derives it from `git describe`); do not hand-maintain a version string.

## Verification

**Commands:**
- `. ~/esp/v6.0.3/esp-idf/export.sh && idf.py -C apps/logic-s3 set-target esp32s3 build` -- expected: exit 0
- `. ~/esp/v6.0.3/esp-idf/export.sh && idf.py -C apps/collector-h2 set-target esp32h2 build` -- expected: exit 0
- `grep -E 'esp-zigbee-lib|cbor' -A2 apps/*/dependencies.lock` -- expected: versions 2.0.4 / 7.0.0
- `gcc -fsyntax-only -x c components/ga_config/include/ga_config.h` -- expected: exit 0
- `grep -E 'SPIRAM_MODE_OCT|LWIP_TCPIP_TASK_AFFINITY_CPU0|FLASHSIZE_16MB|CPU_FREQ_MHZ_240' apps/logic-s3/sdkconfig` -- expected: all present

**Manual checks (if no CLI):**
- Boot banner on real hardware: needs flashed boards; if none are attached, record as not verified.
