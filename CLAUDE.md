# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

RemoteDeckSystem is a multi-component product for ESP32-based remote PC power management. It combines **embedded firmware** (C++/PlatformIO) with several **Windows desktop tools** (.NET 8 WinForms) and a **PDCA documentation workflow** (bkit). The three worlds are developed and versioned together in one repo.

- `RemoteDeck_PC/` — main ESP32 firmware (relay power control, PC-LED / GPIO monitoring, attendance(재부재), scheduling, WOL, OTA).
- `RemoteDeck_Touch/` — secondary ESP32 firmware (PlatformIO, MQTT-only touch node).
- `APITestUtility_v2/integrate_controller/IntegrateController/` — WinForms fleet monitor: polls many devices over HTTP and renders a live grid.
- `APITestUtility_v2/RemoteDeckTest/` — WinForms API tester (HTTP/MQTT/WebSocket/RS485).
- `IPSetupTool/` — WinForms initial-network-setup tool (UDP discovery, requires admin).
- `REST-api/` — small .NET Framework WinForms REST sample.
- `docs/` — user manual + PDCA plan/design/analysis/report + monthly `archive/`.
- `RemoteDeck_Hub/` — empty placeholder (no code yet).

The primary API contract between firmware and every client is the firmware's HTTP+WebSocket server (`:5050`); see `docs/RemoteDeck_PC_Manual.md` for the full API.

## Build / run

**Firmware (`RemoteDeck_PC/`, `RemoteDeck_Touch/`)** — needs PlatformIO:
```bash
cd RemoteDeck_PC
pio run                      # compile
pio run --target upload      # flash over USB
pio run --target uploadfs    # flash SPIFFS (data/: web UI + deviceconfig.json)
build_firmware.bat           # Windows: build + copy to firmware/RemoteDeck_PC_V<ver>_<date>.bin
```
The firmware version is the **single source of truth in `RemoteDeck_PC/data/deviceconfig.json` (`"version"`)**; `build_firmware.bat` derives the output filename from it. Bump it there when releasing.

**.NET tools** — need .NET 8 SDK (dotnet 8.x). All target `net8.0-windows`, published as self-contained single-file `win-x64`:
```bash
cd APITestUtility_v2/integrate_controller/IntegrateController
dotnet build IntegrateController.sln -c Debug     # dev build (0 warnings / 0 errors is the release gate)
publish.bat                                        # release single-file exe -> IntegrateController/publish/
```
`IPSetupTool` and `RemoteDeckTest` follow the same `dotnet publish -c Release -r win-x64 --self-contained -p:PublishSingleFile=true` pattern (see `README.md`).

**Tests:** there is no automated unit-test harness. Verification is (a) `dotnet build` clean (0/0) for .NET, and (b) runtime/field verification against real hardware ("실기기 테스트"). Treat a clean build + confirmed on-device behavior as the done criteria, and record it in the PDCA analysis/report.

## Firmware architecture (`RemoteDeck_PC/src/`)

`main.cpp` is a **single-threaded Arduino orchestrator**: it owns one global instance of each subsystem and wires them together in `setup()`, then services them in `loop()`. Subsystems (`config/`, `control/`, `network/`, `web/`, `serial/`, `utils/`) communicate through direct calls and callbacks registered from `main.cpp` — not a message bus. When adding a feature, the pattern is: add a handler class under the right folder, then wire its getters/callbacks in `main.cpp`.

Several **interfaces run simultaneously against the same state**: Web UI/REST + WebSocket (`:5050`), MQTT (`:1883`), RS485 (9600bps), UDP discovery (`:5051`). State changes fan out to all of them (e.g. `switchMonitor.onChange` triggers a WebSocket status broadcast and can fire a Web Request). Keep new state additions backward-compatible in every interface's payload.

The **attendance (재부재)** feature is the canonical example of a cross-cutting flow: a source (PC-LED or GPIO2) → `SwitchMonitor` → `AttendanceHandler` (ring buffer + fire WebRequest + Logger bridge) → exposed as `/api/status.attendance` and `/api/attendance/history`, and finally consumed by the IntegrateController grid on the client side.

## IntegrateController architecture (the fleet monitor)

`Services/RemoteDeckClient.cs` is the HTTP client that parses `/api/status` into `Models/DeviceStatus`. `DevicePoller` + `LogPoller` poll each configured device every ~5s (staggered), `DeviceStore` holds state, and `UI/MainForm` renders a `DataGridView`; `UI/StatusFormatter` maps raw status to display text. `RemoteDeckClient` parsing must stay **tolerant of unknown/missing fields** so older firmware doesn't regress the client (e.g. a missing `attendance` block renders as "미설정").

## Project-specific gotchas (read before touching these areas)

- **ESP32 + W5500 Ethernet has no working DNS.** MQTT/Web-Request hosts must be resolved to an IP; the firmware carries a hostname→IP workaround (`MQTTTestState.result == -2` is `dns_pending`). Don't assume `hostByName` works on the Ethernet path.
- **`POST /api/reboot` calls `ESP.restart()` before flushing the HTTP response.** Clients must treat a timeout / connection-reset on reboot as **success**, not failure.
- **SPIFFS OTA overwrites the whole filesystem partition.** Any SPIFFS upload / SPIFFS-OTA path must back up and restore `deviceconfig.json` and `schedule.json`, or field devices lose their config.
- **WinForms Designer regeneration silently drops columns.** In `IntegrateController`, the IP grid column and all log-view `ListView` columns are created in code (not the Designer), and cells are addressed by `column.Index` — because the IP column is inserted at runtime and shifts later columns. Preserve this pattern; never rely on a fixed column index. (See memory: WinForms Designer 3중 방어선.)
- **Physical layer first for field issues.** A long cold-boot LAN failure once cost 8 firmware iterations before the real cause turned out to be an unstable power adapter. `NetManager` keeps a minimal defensive line (GOT_IP watchdog → `ESP.restart()`, W5500 SW reset, `ETH.begin` retry); cross-check adapter/power before iterating on firmware.
- **Client version follows the firmware contract.** A client change (e.g. IntegrateController) that consumes a firmware API is tagged with the **consumed firmware version**, not an independent client version.

## PDCA / bkit workflow

This repo uses the **bkit PDCA methodology**; feature work is expected to leave a paper trail, not just code.

- Docs live in `docs/01-plan`, `02-design`, `03-analysis`, `04-report`, and are archived per month under `docs/archive/YYYY-MM/<feature>/` (`plan.md` / `design.md` / `analysis.md` / `report.md`) with a monthly `_INDEX.md`. Match the existing report/analysis format when completing a feature (Context Anchor, Success Criteria table, Match Rate, Carry Items).
- Quality gate: gap-analysis **matchRate ≥ 90%** before completion; otherwise iterate.
- Runtime state is in `.bkit/state/pdca-status.json` — **git-ignored, valid UTF-8, bare-LF, no `\u` escapes.** When editing it programmatically, load/dump with UTF-8 + `ensure_ascii=False` and preserve LF, or you will corrupt the Korean notes of other entries.
- Commit messages: `type(component): 요약` in Korean, e.g. `feat(RemoteDeck_PC): v2.6.2 — …`, `chore(IntegrateController): …`, `docs(archive): …`. Keep the `(component)` scope accurate.

## Defaults & access

Ethernet IP `192.168.1.200`, Web UI `http://<ip>:5050`, auth `admin` / `12345`, MQTT `:1883`, UDP discovery `:5051`. `README.md` and `docs/RemoteDeck_PC_Manual.md` hold the authoritative API and hardware (pinout, relay, wiring) details.
