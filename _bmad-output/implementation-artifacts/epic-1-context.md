# Epic 1 Context: Prototype Confirms Hardware and Zigbee (Прототип подтверждает железо и Zigbee)

<!-- Compiled from planning artifacts. Edit freely. Regenerate with compile-epic-context if planning docs change. -->

## Goal

Prove that the chosen hardware and the hubless Zigbee approach work before the main investment goes in. The epic delivers a buildable monorepo for both firmwares, a host-tested `proto` component whose frame format will not need to change later (including the relay-output message types added for epic 10), and bench experiments for the main risks: S3 talking to an H2 RCP over UART, a distributed-security network formed by routers with no coordinator, delivery through a neighbouring node, Zigbee loss under WiFi/TLS load, and standard Zigbee OTA served from a router. Each risk ends with a "confirmed" or "fallback needed" verdict. The last story is a gate: it writes the results into the architecture and spec (via their journals, `bmad-correct-course` if needed) before Epic 2 can start.

## Stories

- Story 1.1: garageAlarms2.0 repository builds both firmwares
- Story 1.2: `proto` protocol with host (PC) tests
- Story 1.3: S3 + H2 radio form a Zigbee network without a coordinator
- Story 1.4: Delivery through a neighbour
- Story 1.5: Zigbee loss under WiFi and TLS load
- Story 1.6: Zigbee OTA with the server on a router
- Story 1.7: Prototype results into the architecture and spec (gate)

## Requirements & Constraints

- Priority order: never lose an event > never flood the user > everything else. Duplicates are acceptable, losses are not.
- `proto` host tests: pure C, no IDF/FreeRTOS headers, run on the ESP-IDF Linux target with one command, print a gcov report; line and branch coverage of `proto` >= 90 %.
- One Zigbee frame is `GA_ZB_FRAME_MAX_BYTES` (start value 80 B). A channel-state message and an `output_cmd` **including its signature** must each fit in one frame.
- Compatibility N / N−1: unknown fields and types are skipped; a decoder N−1 that forwards a frame N must pass `body` byte-for-byte identical. Fields are never removed or repurposed within a major version.
- Zigbee mesh: distributed security, no coordinator / Trust Center, every mains node is a router; wrong network key must not join; network recovers after S3 restart without manual action.
- Zigbee under WiFi+TLS load (S3 long-polling Telegram + 1 TLS send/s, collector 10 msg/s for 10 min): loss after MAC retries < 1 %, p99 < 500 ms; report loss before/after retries and p50/p99, compared with idle WiFi.
- Mesh relay test: 100 messages via a relay with the route visible; measure re-route recovery time; with no path the loss is logged and the prototype does not hang.
- OTA test: collector finds the router-hosted server, downloads, verifies, reboots (time measured); firmware that cannot join rolls back; if the standard path fails, record why and mark the L2 `bulk` fallback as needed.
- No wait without a "no progress" timeout; every task in the task watchdog (hung task = panic reboot).

## Technical Decisions

- **Toolchain:** ESP-IDF v6.0.3 at `~/esp/v6.0.3/esp-idf` (never `~/Esp32/esp-idf`). Pinned: esp-zigbee-lib 2.0.4, espressif/cbor (TinyCBOR) 7.0.0.
- **Constants (AD-25):** every timeout, interval, buffer size and frame limit lives only in `ga_config.h` (IDF-free, carries `GA_CONFIG_VERSION`); no such literals in `proto` or anywhere else. Relevant to proto: `GA_ZB_FRAME_MAX_BYTES`, `GA_KID_TRANSITION` (7 d), `GA_SEQ_WINDOW` (512), `GA_DEDUP_BOOTS` (4), `GA_LOCAL_RULES_MAX` (16), `GA_CMD_MAX_AGE` (10 min).
- **Component shape (AD-27):** `core/` is a deterministic `step(state, msg, now, rng) -> effects`, no clock reads, no unbounded malloc, only `ga_config.h` and `proto` (with TinyCBOR) allowed; `port/` holds interfaces; `shell/` the FreeRTOS task; `esp_err_t` only in `shell/`/`port/`. Crypto goes through a port: HMAC is a fake in host tests and an mbedTLS adapter on the board. `proto` is built by both firmwares and by the IDF-free core target.
- **Frame (AD-5):** CBOR array `[body, kid, m]`. `body` = byte string containing a CBOR map; `kid` = network-key index; `m` = HMAC with key `kid` over the raw `body` bytes, verified **before** decoding. Tampered `body` byte or wrong `kid` is rejected. During `GA_KID_TRANSITION` both old and new `kid` are accepted, signing uses the new one; an author retrying its unacked event re-signs the same `body` with the current key; a relay never re-encodes and wraps an old-`kid` body in an envelope signed with the current key.
- **Body map keys (short):** `v` version, `t` type, `c` traffic class, `k` criticality, `n` EUI-64, `b` boot, `q` seq, `lw` low-water of the reliable stream, `ts`, `p` payload. Type, class and field codes exist only as `proto` constants.
- **Unknown types (AD-6):** acked, stored and replicated as opaque `body`; routing and criticality come from `c`/`k`, never from `t`.
- **Identity and seq streams (AD-7):** node = EUI-64; event ID = `(node, boot, seq)`. Three distinguishable `seq` streams, each from 0 per boot: broadcast reliable (events, `ack`, `delivered`; its number is the event ID), addressed reliable with a separate counter per `(src, dst)` (config sync, collector channel params, rule commands, `output_cmd`, `local_rules`), unreliable (heartbeat, snapshots, ACK; freshness by `(boot, ts)`, exempt from anti-replay). Derived event ID = `(rule_id, window_start)`. Text form: event `node:boot:seq`, derived `rule_id@window_start`, node as 16 hex.
- **Anti-replay (AD-17):** per-stream window of `GA_SEQ_WINDOW`; base = max(contiguous accepted prefix, `lw` from frame), never decreases; out-of-window frames are not acked; windows kept for the last `GA_DEDUP_BOOTS` boots. Proto must carry `lw` so the window logic in Delivery can use it.
- **Traffic classes (AD-12):** `sensor` → Zigbee; `critical` (alarms, ACK, `delivered`, `ack`, heartbeat, `output_cmd`, `output_state`, `output_intent`) → Zigbee + UDP; `bulk` (config/event sync, `local_rules` table, OTA) → UDP, Zigbee only as rate-limited fallback.
- **Outputs (AD-29), message types proto must define and round-trip:**
  - channel kind `output` (subtype `switch | buzzer`) alongside `binary | numeric | counter | enum`; health `ok|fault|open|short|stale`.
  - `output_cmd{cmd_id, channel_id, level, value: on|off|release|pulse, ts, until}`: addressed reliable, class `critical`. `level` is `protect | manual | auto`. `ts` is an HLC `(wall_ms, counter, EUI-64)`. Commands are ordered by the pair (`ts`, `cmd_id`). Rule commands use `cmd_id = rule_id@event_id` of the cause; manual ones use the `output_intent` event_id.
  - `output_state{value, level, cmd_id, ts, feedback}`: a snapshot; only refusals and `fault` travel as edges.
  - `output_intent`: a replicated `critical` event (level `manual`, HLC `ts`, `cmd_id` = its own event_id; an MQTT-origin intent also carries a fingerprint hash of topic + payload).
  - `local_rules`: versioned table (<= `GA_LOCAL_RULES_MAX`), addressed reliable, `bulk`.
- **Open question Q5 (blocks 1.2):** pick a compact encoding of `cmd_id` (`rule_id@event_id`, where event_id is EUI-64 + boot + seq) and of the HLC (`wall_ms`, counter, EUI-64) so that a full signed `output_cmd` frame (envelope + body + `kid` + `m`) fits `GA_ZB_FRAME_MAX_BYTES`. The spine does not fix the encoding or the HMAC tag length. Record the chosen encoding through the architecture journal; do not decide it silently in code.
- **Conventions:** code, comments and logs in English; frames are printed via `cborjson`; logs use `ESP_LOG*` with the actor tag; messages carry UTC epoch.

## Cross-Story Dependencies

- 1.1 (done) provides the tree, `ga_config.h` and the component shape every other story uses.
- 1.2 needs no hardware and can run in parallel with 1.3. Its frame format, seq streams and HMAC port underpin signing and anti-replay in 2.4, the network simulator in 2.7, UDP in 3.2 and node protection in 6.5. Its output types are the protocol groundwork for epic 10.
- 1.3 is the base for 1.4, 1.5 and 1.6 (a working S3+H2-RCP router network with a joined collector). 1.3/1.4 validate coordinator-less delivery for 2.4.
- 1.6 decides whether collector OTA (6.3) needs the custom L2 `bulk` image transfer.
- 1.7 consumes the reports of 1.3–1.6; tunables measured on the bench flow back into `tunables.md`/`ga_config.h`. Planning docs change only through their `.memlog.md`. Epic 2 is blocked until 1.7 closes every risk.
