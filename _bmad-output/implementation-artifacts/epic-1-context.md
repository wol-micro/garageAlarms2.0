# Epic 1 Context: Prototype Confirms Hardware and Zigbee (Прототип подтверждает железо и Zigbee)

<!-- Compiled from planning artifacts. Edit freely. Regenerate with compile-epic-context if planning docs change. -->

## Goal

Prove that the chosen hardware and the hubless Zigbee approach work before the main investment goes in. The epic delivers a buildable monorepo for both firmwares, a host-tested `proto` component, and bench experiments for the main risks: S3 talking to an H2 RCP over UART, a distributed-security network formed by a router with no coordinator, delivery through a neighbouring node, Zigbee loss under WiFi/TLS load, and standard Zigbee OTA served from a router. Each risk ends with a "confirmed" or "fallback needed" verdict. The last story is a gate: it records the results in the architecture and spec, running `bmad-correct-course` if needed, before Epic 2 can start.

## Stories

- Story 1.1: garageAlarms2.0 repository builds both firmwares
- Story 1.2: `proto` protocol with host (PC) tests
- Story 1.3: S3 + H2 radio form a Zigbee network without a coordinator
- Story 1.4: Delivery through a neighbour
- Story 1.5: Zigbee loss under WiFi and TLS load
- Story 1.6: Zigbee OTA with the server on a router
- Story 1.7: Prototype results into the architecture and spec (gate)

## Requirements & Constraints

- Priority order in every decision: never lose an event > never flood the user > everything else. Duplicates are acceptable, losses are not.
- The Zigbee mesh runs in distributed security mode with no coordinator or Trust Center. Every mains-powered node is a router. A collector must reach a logic node through an intermediate router when it has no direct line of sight.
- Protocol compatibility: version N reads N−1. Unknown fields and types are skipped. A forwarded `body` must stay byte-identical.
- A channel-state message must fit in one Zigbee frame (~80 bytes). The limit lives in `ga_config.h`.
- No wait without a "no progress" timeout. Every task is registered in the task watchdog. TLS starts only after time sync. Prototypes must log a loss, not hang (lesson from 1.x).
- All timeouts, intervals and buffer sizes live only in `components/ga_config/include/ga_config.h`, which carries `GA_CONFIG_VERSION`. Literal values elsewhere are forbidden. Start values come from `tunables.md` (heartbeat 10 s logic / 30 s collector, `stale` = 3 missed heartbeats, T = 45 s, uplink N/M = 60 s/120 s, leader return after 15 min, lease renew/expiry 5/15 min, YIELDED pause 30 s × (1+rank), Telegram send gap 350 ms, etc.). Mark soft-overridable values as defaults.
- Pass criterion for Zigbee under WiFi+TLS load (S3 long-polling Telegram plus one TLS send per second, collector at 10 msg/s for 10 min): loss after MAC retries < 1 %, p99 latency < 500 ms. Report p50/p99 and loss before and after retries, compared against idle WiFi.
- Mesh tests: 100 messages through a relay, with the route visible. Measure and record re-route recovery time when the relay is switched off.
- OTA test: the collector finds the router-hosted server, downloads and verifies the image, and reboots into it (measure the time). Firmware that fails to join the network rolls back. If the standard path fails, record why and mark the fallback (own image transfer over L2, `bulk` class) as needed.
- A collector with the wrong network key must fail to join. The network and collector must recover automatically after S3 restarts.
- Language: code, comments and logs in English. User-facing text in Russian.
- Bench hardware: 2 × ESP32-S3 N16R8 (WeAct) as logic nodes, ESP32-H2 (Waveshare MINI-1-N4) as RCP for each S3 and as 2 collectors (plus a spare), SPI FRAM breakouts (logic nodes only), independently switchable 5 V USB supplies, and an optional logic analyser for S3↔H2 UART debugging. The 1.x box stays in service and is not part of the bench.

## Technical Decisions

- **Toolchain/pins:** ESP-IDF v6.0.x (installed: v6.0.3 at `~/esp/v6.0.3/esp-idf`; do not use `~/Esp32/esp-idf`). Targets esp32s3 (`apps/logic-s3`) and esp32h2 (`apps/collector-h2`). Pin `esp-zigbee-lib` 2.0.4 and `espressif/cbor` 7.0.0 in `idf_component.yml`. Later pins: `network_provisioning` 1.2.5, `espressif/mqtt` 1.0.0. v6.1 waits until esp-zigbee-lib supports it.
- **Repo tree:** `components/` = `ga_config`, `proto`, `mesh`, `delivery`, `membership`, `storage`, `sensors`, `sys` (both firmwares) plus `config`, `rules`, `notify`, `ui` (logic node only). `apps/logic-s3` is the host build, and its H2 runs Espressif's ready-made RCP firmware. `apps/collector-h2` runs the native Zigbee stack. Code from 1.x is not ported.
- **Boot log** (1.1): role, EUI-64, firmware version, `GA_CONFIG_VERSION`. The README holds build/flash/monitor commands for both chips, and the CLAUDE.md Build section must be updated too.
- **Actor model:** one FreeRTOS task per actor, which solely owns its state, with one input queue. Requests and replies are matched by `corr_id`. Upward and peer communication uses subscribed events only. Requests follow the dependency diagram (e.g. Delivery → Mesh, everything → Storage, Storage depends on nothing). Stubs should respect this.
- **Frame format (`proto` is the only source of protocol):** a CBOR array `[body, kid, m]`. `body` is a byte string holding a CBOR map with short keys `v` version, `t` type, `c` traffic class, `k` criticality, `n` EUI-64, `b` boot, `q` seq, `ts`, `p` payload. `kid` is the network-key index. `m` is the HMAC with key `kid` over the raw `body` bytes, checked **before** decoding. A wrong `kid` or a tampered byte is rejected. During key rotation both old and new `kid` are accepted and the new one is used for signing. Relays never re-encode. Unknown event types are ACKed, stored and forwarded as opaque `body`. Routing and criticality come from `c`/`k`, not from the type. Type, class and field codes are constants from `proto` only. Frames are printed via `cborjson`.
- **IDs:** node = Zigbee EUI-64 (16 hex). Event = `node:boot:seq`. `boot` is a persistent counter, incremented and saved before the first send. `seq` starts at 0 each boot, with one counter per node. Anti-replay uses a `seq` bitmap per `(node, boot)`.
- **Host tests:** `proto` (and later `delivery`) tests build for the ESP-IDF Linux target and run with a single command.
- **Mesh/transport:** Zigbee sits behind the `Mesh` interface (H2 RCP over UART on the logic node, native stack on the collector). Class-based routing: `sensor` goes over Zigbee, `critical` over Zigbee and UDP together, `bulk` over UDP, falling back to Zigbee only rate-limited. Network parameters and key come from provisioning.
- **OTA (target design):** only one node updates at a time. Collectors update via the Zigbee OTA Upgrade cluster with `LEADER` as server. New firmware confirms itself only after joining the network, otherwise it rolls back. The open question to close in Epic 1: does an OTA server on a router work in a distributed network?
- **Conventions:** `esp_err_t` at component boundaries. Network errors trigger exponential-backoff retry, not abort. `ESP_LOG*` uses the actor tag. Actors use `PascalCase`, ESP-IDF components `snake_case`. Messages carry UTC epoch.

## Cross-Story Dependencies

- 1.1 comes first: every other story builds on its tree and on `ga_config.h`.
- 1.2 needs only 1.1 and no hardware. It can run in parallel with 1.3.
- 1.3 is the base for 1.4, 1.5 and 1.6 (working S3+H2 RCP router network with a collector joined).
- 1.7 consumes the reports from 1.3–1.6. Architecture and spec changes go through their `.memlog.md` journals, never by hand edits. Epic 2 is blocked until 1.7 closes every risk.
- Outbound effects: 1.3/1.4 validate FR8 for Epic 2 (story 2.4). 1.6 decides whether Epic 5 (story 5.3) needs the L2 `bulk` fallback. The `proto` tests in 1.2 underpin signing and anti-replay in 2.4, 3.2 and 5.5. Tunables measured on the bench feed back into `ga_config.h`/`tunables.md`.
