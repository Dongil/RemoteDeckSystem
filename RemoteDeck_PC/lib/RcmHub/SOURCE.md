# RcmHub (vendored)

Copied unchanged from `rcm-api/clients/esp32/lib/RcmHub/` (https://github.com/Dongil/rcm-api, tag v1.1.0, commit ee27a69).
Do not edit here only: any fix must also go to rcm-api as a PR (handoff/SMARTDECK.md §0).

## History

| Date | Change | Upstream |
|------|--------|----------|
| 2026-11-09 | Initial import (v1.0.2, commit 3c1791b) | — |
| 2026-11-10 | `RcmHub::setTap()` — optional observer for sent/received frames and socket connect/disconnect (no behaviour change). Used by `HubBridge` for the web UI traffic log (`GET /api/hub`; hello `auth.sig` is never recorded). | rcm-api PR #1 merged → tag v1.0.4 |
| 2026-11-11 | v1.1.0 rcm-power-diag: `RcmHub::peerIp()` (welcome `pair.peer.regIp`), `RcmDeckIo.peerNet` → `deck.state.peer {net, ip}`. Used by `HubBridge` PeerProbe (ICMP ping of the paired PC). | rcm-api PR #10 merged (5420871) → tag v1.1.0 (ee27a69). No local changes remain. |
