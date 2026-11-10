# RcmHub (vendored)

Copied unchanged from `rcm-api/clients/esp32/lib/RcmHub/` (https://github.com/Dongil/rcm-api, tag v1.0.4, commit d4cbe79).
Do not edit here only: any fix must also go to rcm-api as a PR (handoff/SMARTDECK.md §0).

## History

| Date | Change | Upstream |
|------|--------|----------|
| 2026-11-09 | Initial import (v1.0.2, commit 3c1791b) | — |
| 2026-11-10 | `RcmHub::setTap()` — optional observer for sent/received frames and socket connect/disconnect (no behaviour change). Used by `HubBridge` for the web UI traffic log (`GET /api/hub`; hello `auth.sig` is never recorded). | rcm-api PR #1 merged → tag v1.0.4. No local changes remain. |
