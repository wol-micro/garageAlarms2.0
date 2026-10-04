- source_spec: `_bmad-output/implementation-artifacts/spec-1-1-repo-builds-both-firmwares.md`
  summary: Story 1.3 — once the S3↔H2 RCP link is up, log the real Zigbee EUI-64 of the logic node (the 1.1 boot banner shows only a provisional MAC-derived value).
  evidence: Decision on 2026-10-02 in story 1.1: the S3 has no 802.15.4 radio, so the real EUI-64 is only readable from the RCP.
- source_spec: `_bmad-output/implementation-artifacts/spec-1-1-repo-builds-both-firmwares.md`
  summary: CLAUDE.md "Source of truth" table still cites the spine as AD-1…AD-25; it now has AD-1…AD-28.
  evidence: Blind-hunter review of story 1.1; agent-context files are changed only with the human's consent.
- source_spec: `_bmad-output/implementation-artifacts/spec-1-1-repo-builds-both-firmwares.md`
  summary: Decide whether the task watchdog panics (resets the node) on a hung task — `CONFIG_ESP_TASK_WDT_PANIC` is n in both apps.
  evidence: AD-3 registers every task in the TWDT; with panic off a hang is only logged. Decide with actor TWDT registration in story 2.1.
- source_spec: `_bmad-output/implementation-artifacts/spec-1-1-repo-builds-both-firmwares.md`
  summary: Add partition tables — OTA slots for both apps and the `zb_storage`/`zb_fct` partitions esp-zigbee-lib expects.
  evidence: Both apps build with the default single-app layout; needed by story 1.3 (Zigbee NVS) and 6.2 (OTA), and must land before production boards are flashed.
- source_spec: `_bmad-output/implementation-artifacts/spec-1-1-repo-builds-both-firmwares.md`
  summary: Move the provisional MAC→EUI-64 derivation into `sys/core/` with a host test.
  evidence: Verification-gap review: the derivation is pure but untestable inside the IDF banner function; do it once story 1.2's host runner exists.
