# rest-api-sample — Gap Analysis Report

## Executive Summary

| Item | Value |
|------|-------|
| Feature | rest-api-sample |
| Design Doc | `docs/02-design/features/rest-api-sample.design.md` |
| Plan Doc | `docs/01-plan/features/rest-api-sample.plan.md` |
| Implementation | `REST-api/Form1.cs`, `REST-api/Form1.Designer.cs`, `REST-api/REST-api.csproj` |
| Analysis Date | 2026-05-11 |
| **Match Rate** | **100%** (15/15 verifiable items, all Do-phase fixes captured) |
| **Verdict** | **PASS** — proceed to Report phase |

## Overall Scores

| Category | Score | Status |
|----------|:-----:|:------:|
| Design Match (FR/NFR) | 100% | PASS |
| API Contract Match | 100% | PASS |
| UI Mapping Match | 100% | PASS |
| Error Handling Match | 100% | PASS |
| Do-Phase Fix Integration | 100% | PASS |
| **Overall** | **100%** | **PASS** |

---

## 1. Functional Requirements Match Table

| ID | Requirement | Design Loc. | Impl Loc. (file:line) | Status |
|----|-------------|-------------|-----------------------|:------:|
| FR-01 | Start button → connect with IP/Port/ID/PW | §1.2, §5.2 | `Form1.cs:62-99` (`btnConn_Click`) | PASS |
| FR-02 | 1s polling on success | §1.2, NFR-03 | `Form1.cs:19` (Interval=1000), `Form1.cs:93` (`_pollTimer.Start()`) | PASS |
| FR-03 | Connect/auth fail → log + buttons stay disabled | §6 | `Form1.cs:97` (catch), `Form1.cs:124-128` (non-2xx), `Form1.cs:106` (SetControlsEnabled(false)) | PASS |
| FR-04 | On success → enable Relay1/2 buttons | §1.2 | `Form1.cs:91` (`SetControlsEnabled(true)`) | PASS |
| FR-05 | Relay button → POST /api/relay | §3.2 | `Form1.cs:143-157`, `Form1.cs:159-174` | PASS |
| FR-06 | ON-button bg: ON=Red, OFF=Control | §2.2 | `Form1.cs:187-195` (UpdateRelayUI) | PASS |
| FR-07 | Log all HTTP with timestamp + req/resp | §5.2 | Log calls @ `:84,119,125,129,137,165,170,173`; `Form1.cs:203-216` | PASS |
| FR-08 | Start/Stop toggle, Stop = stop polling + disable | §1.2, §5.2 | `Form1.cs:64`, `Form1.cs:101-110` | PASS |
| FR-09 | Default values auto-fill | §2.1, §5.2 | `Form1.cs:53-59` (Form1_Load) | PASS |
| FR-10 | Pulse button is event-type, not color-tracked | §2.2 | `Form1.cs:187-195` — only ON buttons colored | PASS |

## 2. Non-Functional Requirements Match Table

| ID | Requirement | Target | Impl | Status |
|----|-------------|--------|------|:------:|
| NFR-01 | .NET Framework 4.8 + 0 NuGet | required | `REST-api.csproj:11`, `:45`; no PackageReference | PASS |
| NFR-02 | UI responsiveness via async/await | required | All HTTP async; fire-and-forget click handlers | PASS |
| NFR-03 | 1s poll, 3s HTTP timeout | recommended | `Form1.cs:19`, `Form1.cs:77` | PASS |
| NFR-04 | Auto-scroll + 1000-line cap | recommended | `Form1.cs:23,206-215` | PASS |
| NFR-05 | Single-file (Form1.cs) implementation | required | All logic in Form1.cs; Designer untouched | PASS |

## 3. API Contract Verification

| Endpoint / Aspect | Design | Implementation | Status |
|-------------------|--------|----------------|:------:|
| `GET /api/status` URL | §3.1 | `Form1.cs:120` | PASS |
| Basic Auth header | base64(id:pw) | `Form1.cs:80-82` | PASS |
| `POST /api/relay` URL | §3.2 | `Form1.cs:167` | PASS |
| Relay payload | `{"relay":N,"state":"on/off"}` | `Form1.cs:145-147` | PASS |
| Pulse payload | `{"cmd":"pulse","relay":N}` | `Form1.cs:153-155` | PASS |
| Content-Type | application/json | `Form1.cs:166` | PASS |
| 401/non-2xx handling | log + OnFailure | `Form1.cs:123-128` | PASS |
| 3-consecutive-fail auto-disconnect | §6 | `Form1.cs:176-184`, MAX_FAILURES=3 | PASS |

## 4. Do-Phase Fixes Verification

| # | Fix | Verification | Status |
|---|-----|-------------|:------:|
| 1 | DTO `relay1/relay2` as `bool`; caller maps `? 1 : 0` | `Form1.cs:220-224` DTO, `Form1.cs:47,92` callers | PASS |
| 2 | `UpdateRelayUI` toggles `UseVisualStyleBackColor = r != 1` before BackColor | `Form1.cs:191,193` | PASS |
| 3 | `_http` recreated per Start; old instance disposed; null-guards in Fetch/Post | `Form1.cs:74-79,116,161` | PASS |
| 4 | Start: `SetControlsEnabled(true)` BEFORE `UpdateRelayUI` | `Form1.cs:91→92` | PASS |

## 5. Implementation Order Verification (Design §7)

| Step | Item | Status |
|------|------|:------:|
| 1 | csproj `System.Web.Extensions` | PASS |
| 2 | Form1.cs skeleton (fields, WireUp, Form1_Load) | PASS |
| 3 | `Log()` helper | PASS |
| 4 | `btnConn_Click` + Basic Auth | PASS |
| 5 | `FetchStatusAsync` + DTO + `UpdateRelayUI` | PASS |
| 6 | 1s polling | PASS |
| 7 | `SendRelayAsync` / `SendPulseAsync` + handlers | PASS |
| 8 | `OnFailure` + auto-disconnect | PASS |
| 9 | Form title change | PASS (`"RemoteDeck HTTP Sample"`) |

## 6. Gap List

### Missing
*(none)*

### Added (unsanctioned)
*(none)*

### Changed (intentional, documented deviations)

| # | Item | Design | Implementation | Severity | Justification |
|---|------|--------|----------------|:--------:|---------------|
| C-1 | `HttpClient` lifecycle | `static readonly _http` initialized once | Per-Start instance with Dispose | INFO | `HttpClient.BaseAddress` / `DefaultRequestHeaders` are immutable after first request — needed to support reconnect with different params. Documented at `Form1.cs:16-18`. |
| C-2 | `StatusResponse` field types | `int relay1, relay2` | `bool relay1, relay2` | INFO | Firmware emits JSON booleans. Callers convert `? 1 : 0` so `UpdateRelayUI(int,int)` signature is preserved. Documented at `Form1.cs:219`. |
| C-3 | `UpdateRelayUI` body | Only BackColor | Also toggles `UseVisualStyleBackColor` | INFO | Themed Windows silently override BackColor when `UseVisualStyleBackColor=true`. Documented at `Form1.cs:189-190`. |
| C-4 | `FetchStatusAsync` signature | `Task<bool>` | `Task<StatusResponse>` nullable | INFO | Caller decides when to apply UI so Start can call `SetControlsEnabled(true)` BEFORE `UpdateRelayUI(...)`. |

All four are intentional bug-fixes discovered during Do phase, commented in code, and do not count against Match Rate.

## 7. Recommended Actions

### Immediate
*(none)*

### Design Document Refresh (low priority, non-blocking)

Update `docs/02-design/features/rest-api-sample.design.md` to mirror the four documented deviations:

1. **§4.1 DTO** — `int relay1/relay2` → `bool relay1/relay2`; note firmware emits booleans.
2. **§5.2 Fields** — `HttpClient _http` per-Start instance with Dispose; document immutability rationale.
3. **§5.2 `UpdateRelayUI`** — add `UseVisualStyleBackColor = r != 1`; note themed-Windows behaviour.
4. **§5.2 `FetchStatusAsync`** — return type `Task<StatusResponse>` (nullable); caller controls UI application.

## 8. Conclusion

**Match Rate: 100% (PASS).**

- 10/10 FRs implemented
- 5/5 NFRs satisfied
- 8/8 API contract aspects verified
- 4/4 Do-phase fixes captured
- 0 missing, 0 unsanctioned additions
- 4 intentional documented deviations (all warranted)

**Next**: `/pdca report rest-api-sample`
