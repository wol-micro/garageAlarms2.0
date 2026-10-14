- source_spec: `_bmad-output/implementation-artifacts/spec-1-1-repo-builds-both-firmwares.md`
  summary: Story 1.3 — once the S3↔H2 RCP link is up, log the real Zigbee EUI-64 of the logic node (the 1.1 boot banner shows only a provisional MAC-derived value).
  evidence: Decision on 2026-10-02 in story 1.1: the S3 has no 802.15.4 radio, so the real EUI-64 is only readable from the RCP.
- source_spec: `_bmad-output/implementation-artifacts/spec-1-1-repo-builds-both-firmwares.md`
  summary: Add partition tables — OTA slots for both apps and the `zb_storage`/`zb_fct` partitions esp-zigbee-lib expects.
  evidence: Both apps build with the default single-app layout; needed by story 1.3 (Zigbee NVS) and 6.2 (OTA), and must land before production boards are flashed.
- source_spec: `_bmad-output/implementation-artifacts/spec-1-1-repo-builds-both-firmwares.md`
  summary: Move the provisional MAC→EUI-64 derivation into `sys/core/` with a host test.
  evidence: Verification-gap review: the derivation is pure but untestable inside the IDF banner function; do it once story 1.2's host runner exists.
- source_spec: `_bmad-output/planning-artifacts/architecture/architecture-garageAlarms-2026-09-29/ARCHITECTURE-SPINE.md`
  summary: UX for the network key change — web actions «Сменить ключ сети» / «Завершить смену ключа», key pack with two keys and `T_end`, «введите узел заново» on a cut-off node — goes into EXPERIENCE.md via bmad-ux before story 6.5.
  evidence: AD-5 key-change rule (owner decision 2026-10-07); EXPERIENCE.md node-setup flow assumes one key.
