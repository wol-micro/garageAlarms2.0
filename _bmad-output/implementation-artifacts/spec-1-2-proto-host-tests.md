---
title: 'Story 1.2: proto protocol with host tests'
type: 'feature'
created: '2026-10-05'
status: 'draft'
route: 'full'
route_source: 'auto'
review: ''
review_source: ''
lenses_ran: []
review_loop_iteration: 0
context:
  - '{project-root}/_bmad-output/implementation-artifacts/epic-1-context.md'
  - '{project-root}/_bmad-output/specs/spec-garage-alarms-2/wire-format.md'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** `components/proto` is an empty stub; nothing defines the signed frame, the envelope, the seq streams or the message types, so no later story can exchange or test messages, and the 80-byte Zigbee limit (Q5) is still unproven for `output_cmd`.

**Approach:** Implement `proto` as pure C over TinyCBOR: frame `[body, kid, m]` with HMAC through a port, the AD-5 envelope, type/class/field codes, encoders/decoders for channel state and the four AD-29 output messages, plus a one-command host test runner on the ESP-IDF Linux target with a gcov gate of ≥ 90 % lines and branches.

## Boundaries & Constraints

**Always:**
- `proto/src` and `proto/include` include only libc, `ga_config.h` and TinyCBOR — no IDF, FreeRTOS or mbedTLS headers (AD-27).
- HMAC is computed only through a port struct; host tests use a deterministic fake; a separate mbedTLS adapter (HMAC-SHA256, truncated) is the board implementation and is also tested on the Linux target against RFC 4231 vectors.
- Signature is checked over the raw `body` bytes before any decoding; forwarding never re-encodes `body` (AD-5).
- Extra trailing elements of the envelope and payloads, and unknown types, decode without error and are preserved for forwarding (AD-6); a reserved (4th) stream or class value in `hdr` is `PROTO_ERR_FORMAT`.
- Tunable limits come from `ga_config.h` (`GA_ZB_FRAME_MAX_BYTES`, `GA_LOCAL_RULES_MAX` with a static assert ≤ 16, `GA_OUTPUT_MAX_ON_MAX` as the `dur_s` bound, `GA_PROTO_MAC_BYTES`); fixed format bounds of `wire-format.md` (kid ≤ 23, Zigbee type codes ≤ 23, counters ≤ 65535) are named `PROTO_*` constants in `proto.h`, never bare literals.
- Three seq streams are distinguishable in the envelope (AD-7); `lw` is a field.
- No heap use in encode/decode; caller supplies buffers.

**Never:**
- No anti-replay windows, ACK/retry logic, key storage or kid rotation policy (Delivery 2.4, Sys 2.2) — `proto` only carries `kid`, `q`, `lw`.
- No payload schemas beyond channel state and the four output messages; other types get only their type codes.
- No changes to actor components or apps beyond linking `proto`.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Round trip | envelope + each payload type | decoded fields equal encoded | — |
| Tampered body | one byte of `body` flipped | verify fails, nothing decoded | `PROTO_ERR_MAC` |
| Wrong kid | `kid` with no key in the port | verify fails | `PROTO_ERR_KID` |
| Newer version | body with extra trailing elements and unknown `t` | decodes; unknown kept opaque; forwarded `body` byte-identical | — |
| Too big | encode exceeds `GA_ZB_FRAME_MAX_BYTES` for a Zigbee-bound type | encode returns error, buffer untouched | `PROTO_ERR_SIZE` |
| Truncated / garbage | random or cut bytes into decode | error, no crash, no out-of-bounds read | `PROTO_ERR_FORMAT` |
| Size proof | every Zigbee-bound message with all fields at their bounds | exact sizes of `wire-format.md` (cmd 70, state 77, intent 71, channel 65, rules part 75), each ≤ `GA_ZB_FRAME_MAX_BYTES` | test fails if not |
| Field out of bounds | e.g. `kid` > 23, `dur_s` > 43200, Zigbee type code > 23 | encode refuses | `PROTO_ERR_SIZE` |
| Pulse without duration | `output_cmd` with `pulse`, `dur_s` = 0 | encode refuses, decode rejects | `PROTO_ERR_FORMAT` |

**Decisions (human, 2026-10-05):**
- HMAC tag `m` = 8 B: HMAC-SHA256 truncated to 64 bit (`GA_PROTO_MAC_BYTES` in `ga_config.h`).
- Envelope and payloads are positional CBOR arrays (human, 2026-10-06, replacing the integer-keyed map after exact worst-case sizing): `body = [hdr, t, n, b, q, lwd, ts, p]`; new fields only appended; layouts, bounds and the ≤ 80 B proof are in `wire-format.md`.
- Compact `output_cmd`: `cmd_id` = first 8 B of SHA-256 over the canonical id text (`rule_id@event_id`, the intent `event_id`, or `topic ‖ 0x00 ‖ id` for MQTT with `id`); the command HLC wall time is the envelope `ts`, the payload carries only the HLC counter; order is (`ts`, HLC counter, `cmd_id`), counter overflow bumps `ts` by 1 ms; `dur_s` is a duration in seconds: deadline = `ts` + `dur_s` when the receiver knows time, else from receipt on its monotonic clock. SHA-256 goes through the same crypto port as HMAC.

</frozen-after-approval>

## Code Map

- `components/proto/{CMakeLists.txt,idf_component.yml,include/proto.h,src/.gitkeep,test/.gitkeep}` -- stub from 1.1; cbor 7.0.0 already pinned. Replace header, add `src/*.c`, register SRCS and `REQUIRES ga_config`, `PRIV_REQUIRES espressif__cbor`.
- `components/ga_config/include/ga_config.h` -- `GA_ZB_FRAME_MAX_BYTES 80U`, `GA_LOCAL_RULES_MAX`; add any new proto constants here (with static asserts), keep header IDF-free.
- `apps/*/managed_components/espressif__cbor/` -- TinyCBOR supports target `linux` (see its `host_test/`, `CMakeLists.txt` linux branch); reuse its host_test project layout (`set(COMPONENTS main)`, Unity, `WHOLE_ARCHIVE`).
- `~/esp/v6.0.3/esp-idf/components/{linux,unity,mbedtls}` -- available for the Linux target.
- Coverage: `gcov` is installed; `uvx gcovr` (8.6) gives line + branch report and `--fail-under-line/--fail-under-branch`.
- Spine AD-5, AD-6, AD-7, AD-12, AD-15, AD-17, AD-29 -- field names and semantics; epic-1-context lists them.
- Do not touch `components/sys/shell/sys_banner.c` (MAC→EUI host test stays deferred).

## Tasks & Acceptance

**Execution:**
- [ ] `_bmad-output/specs/spec-garage-alarms-2/tunables.md` (via its memlog) -- add a machine-readable column with one start value per constant in base units (ms, B, count); ranges and formulas resolved to one start value (formula stays in the description); `GA_FRAM_*` and `GA_TASK_*` expanded into their own tables; «задаётся в истории N.x» rows are name-only.
- [ ] `components/ga_config/include/ga_config.h`, `tools/check_tunables.py` -- add every `tunables.md` constant still missing (outputs, MQTT, `GA_PROTO_MAC_BYTES`, `GA_NET_KEY_BYTES`, `GA_COLLECTOR_CRIT_MERGE_PERSIST`, …; rows marked «задаётся в истории 3.x» get a placeholder value with a `/* start value set in story 3.x */` comment) and a check that every `GA_*` name in `tunables.md` exists in `ga_config.h` with the same start value (name-only rows: presence); `PROTO_MIN_VERSION` and other tunable protocol bounds also live in `ga_config.h`; the check also flags `pdMS_TO_TICKS(<digits>)` and similar time/size literals outside `ga_config.h` -- owner requirement 2026-10-07: all important constants in one global file, drift caught automatically.
- [ ] `components/proto/include/proto.h` -- public API: codes for types/classes/criticality/stream kind/levels/values/channel kinds, `proto_env_t`, frame sign/verify, envelope encode/decode, payload encode/decode for channel state, `output_cmd`, `output_state`, `output_intent`, `local_rules` part, error enum -- single protocol source (AD-5).
- [ ] `components/proto/include/proto_crypto_port.h` -- port struct: `mac(ctx, kid, data, len, out[GA_PROTO_MAC_BYTES])` returning ok / unknown kid, and `sha256(ctx, data, len, out[32])` -- crypto behind a port (AD-27).
- [ ] `components/proto/src/{frame.c,envelope.c,payload_*.c}` -- implementation over TinyCBOR, fixed buffers.
- [ ] `components/proto/port/proto_crypto_mbedtls.c` -- board adapter: HMAC-SHA256 truncated to `GA_PROTO_MAC_BYTES` and SHA-256 via mbedTLS, key looked up by `kid` through a callback.
- [ ] `components/proto/test/host/` -- Linux-target project: Unity tests for every I/O matrix row, AC below, hash test vectors for `cmd_id` and fingerprint (canonical CBOR arrays), HLC counter overflow, empty and 16-rule `local_rules`, fuzz-style decode of random/truncated input (fixed seed), RFC 4231 vectors for the adapter, a size test printing worst-case byte counts.
- [ ] `tools/host_test.sh` -- one command: export IDF, build+run `proto` host tests on Linux target with coverage flags, `uvx gcovr` over `components/proto/src` with fail-under 90 line and branch, plus `gcc -fsyntax-only` of every `proto/src/*.c` with only `ga_config` and TinyCBOR include paths.
- [ ] `CLAUDE.md`, `README.md` Build section -- add the host test command.

**Acceptance Criteria:**
- Given `tools/host_test.sh`, when run on a clean tree, then all tests pass, the gcov report is printed and `proto` line and branch coverage are each ≥ 90 %.
- Given the envelope, when encoded, then `hdr` (`v`, stream, `c`, `k`), `t, n, b, q, lwd, ts, p` round-trip and the stream kind (broadcast reliable / addressed reliable / unreliable) is recoverable.
- Given both firmwares, when built, then they link `proto` and the mbedTLS adapter without warnings.

## Design Notes

`wire-format.md` is the contract: field order, bit packing of `hdr`, `level|value`, part counters, and per-field bounds. Implement bounds as checks in the encoders, not as comments. `local_rules` goes one rule per part `{version, part, parts, rule}`, ≤ `GA_LOCAL_RULES_MAX` parts; the collector applies a version only when all parts arrived. MQTT key helpers (`cmd_id` from `id`, fingerprint) live in `proto` so logic nodes compute them identically.

## Verification

**Commands:**
- `tools/host_test.sh` -- expected: exit 0, coverage lines/branches ≥ 90 % for `components/proto/src`
- `python3 tools/check_tunables.py` -- expected: exit 0, every `tunables.md` constant present in `ga_config.h` with the same value
- `. ~/esp/v6.0.3/esp-idf/export.sh && idf.py -C apps/logic-s3 build && idf.py -C apps/collector-h2 build` -- expected: exit 0, no warnings from `proto`
