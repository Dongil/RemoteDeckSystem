# RcmHub (vendored)

Copied from `rcm-api/clients/esp32/lib/RcmHub/` (https://github.com/Dongil/rcm-api, tag v1.0.2, commit 3c1791b).
Do not edit here only: any fix must also go to rcm-api as a PR (handoff/SMARTDECK.md §0).

## Local changes (pending upstream)

| Date | Change | Upstream |
|------|--------|----------|
| 2026-11-10 | `RcmHub::setTap()` — optional observer for sent/received frames and socket connect/disconnect (no behaviour change). Used by `HubBridge` for the web UI traffic log (`GET /api/hub`). | rcm-api PR #1 (branch `esp32-rcmhub-tap`, commit f9e64a4), open |
