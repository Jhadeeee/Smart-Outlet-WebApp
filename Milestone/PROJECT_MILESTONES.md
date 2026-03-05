# 🔌 Smart Outlet System — Project Milestones

**Last Updated:** March 5, 2026 (v8.0.0 — WiFi Scanner, Server URL Auto-Format, UI Polish)

---

## System Architecture

![Smart Outlet System Architecture](Smart%20outlet%20system%20Architecture.png)

---

## Component Status

| Component                | Version | Status         | Documentation                                                                 |
|:-------------------------|:--------|:---------------|:------------------------------------------------------------------------------|
| SmartOutlet Firmware     | v5.3.1  | ✅ Stable      | [FIRMWARE_DOCS.md](Smart%20Outlet%20Device%20dev/Documentation/FIRMWARE_DOCS.md) |
| CCU Firmware (ESP32)     | v8.0.0  | ✅ Stable      | [FIRMWARE_DOCS.md](Central%20Control%20Unit%20dev/Documentation/FIRMWARE_DOCS.md) |
| Smart-Outlet-WebApp      | v8.0.0  | ✅ Stable      | [WEBAPP_DOCS.md](Smart-Outlet-WebApp%20dev/Documentation/WEBAPP_DOCS.md)            |
| Outlet Breaker (SCT013)  | v8.0.0  | ✅ Stable      | [FIRMWARE_DOCS.md](Central%20Control%20Unit%20dev/Documentation/FIRMWARE_DOCS.md) |

---

## Milestone Log

### CCU Firmware (ESP32)

| Date       | Version | Milestone                                                                 |
|:-----------|:--------|:--------------------------------------------------------------------------|
| 2026-03-05 | v8.0.0  | WiFi Scanner in Captive Portal & Dashboard Settings, frosted blur UI, Server URL auto-formatting |
| 2026-03-04 | v5.0.0  | Partition scheme change — Huge APP (3MB No OTA / 1MB SPIFFS), flash usage down from 91% to ~38% |
| 2026-03-04 | v4.3.0  | Breaker ADC fix — blocking readFresh() immune to WiFi noise, direct BreakerMonitor pointer |
| 2026-03-03 | v4.2.0  | Focus Device — cloud loop only reads expanded device, auto-tare breaker   |
| 2026-03-03 | v4.1.0  | Direct API routes for Django, event-driven serial output, breaker cache   |
| 2026-02-24 | v4.0.0  | Main breaker monitoring — SCT013 integration, dashboard UI, cut-all/per-device |
| 2026-02-23 | v3.0.0  | Added developer documentation and user testing guide                      |
| 2026-02-23 | v2.x    | Bug fixes — current routing by socket ID, Device ID ACK detection         |
| 2026-02-10 | v2.0.0  | Dashboard UI overhaul — device list, toggle switches, auto-poll, REST API |

### SmartOutlet Firmware (PIC16F88)

| Date       | Version | Milestone                                                                 |
|:-----------|:--------|:--------------------------------------------------------------------------|
| 2026-02-10 | v5.3.1  | Multi-device support — configurable Device IDs (0xFE, 0xFD)              |
|            | v5.x    | Overload protection with auto-retry, config mode via RB3 hold             |
|            | v5.x    | NC relay wiring, dual ACS712 sensors, factory reset via 3× press          |

### Smart-Outlet-WebApp

| Date       | Version | Milestone                                                                 |
|:-----------|:--------|:--------------------------------------------------------------------------|
| 2026-03-05 | v8.1.0  | Live breaker line chart (Chart.js), outlet card state caching, toggle visual persistence, breaker noise floor 250→280 mA |
| 2026-03-05 | v8.0.0  | Version sync with system-wide v8.0.0 bump, documentation updates         |
| 2026-03-04 | v7.0.0  | Expandable breaker monitor with color indicators, noise floor filters (0-100mA outlet, 0-250mA breaker), real-time badge updates, Event History page with filters + card/table views |
| 2026-03-03 | v1.3.0  | Focus Device — expand/collapse outlets, disabled controls when collapsed  |
| 2026-03-03 | v1.2.0  | Direct ESP32 communication, EventLog model, mA display, toggle loading    |
| 2026-02-24 | v1.1.0  | Fully functional Test Dashboard — 2-way sync queueing, Telemetry log UI   |
| 2026-02-24 | v1.0.0  | Added comprehensive developer documentation `WEBAPP_DOCS.md`              |
| 2026-02-12 | v1.0.0  | Basic WebApp — Auth, Chatbot, WebSocket layer, API routes, Local Dev Mode |

---

## Roadmap

- [x] Outlet Breaker — SCT013-100A main load monitoring via ESP32
- [x] WebApp full cloud integration — ESP32 ↔ Django server data sync
- [x] Direct ESP32 communication — relay commands via HTTP (bypasses polling queue)
- [x] Focus Device — only read/control the expanded outlet (mirrors local dashboard)
- [x] Expandable breaker monitor — color-coded load card, per-outlet cut buttons
- [x] Noise floor filters — clamp sensor noise to 0 (PIC: 0-100mA, SCT013: 0-280mA)
- [x] Event History page — filterable event log with card/table views
- [x] Real-time status badges — Active/Inactive counts update live via WebSocket
- [ ] Auto cut-off — automatically kill all outlets when breaker threshold exceeded
- [x] Online Dashboard — CRUD for outlets, threshold config, AI chat panel
- [ ] Persistent device storage on ESP32 (SPIFFS/NVS instead of RAM)
- [ ] OTA firmware updates for ESP32 (currently sacrificed for flash space via Huge APP partition)

---

## Documentation Index

| Document                      | Location                                                                                          |
|:------------------------------|:--------------------------------------------------------------------------------------------------|
| System Architecture           | [Smart outlet system Architecture.png](Smart%20outlet%20system%20Architecture.png)                |
| PIC Firmware Docs             | [FIRMWARE_DOCS.md](Smart%20Outlet%20Device%20dev/Documentation/FIRMWARE_DOCS.md)                   |
| PIC Circuit Diagram           | [Smart outlet_Actual_Circuit_v5.2.0.PDF](Smart%20Outlet%20Device%20dev/Images/Smart%20outlet_Actual_Circuit_v5.2.0.PDF) |
| CCU Firmware Docs             | [FIRMWARE_DOCS.md](Central%20Control%20Unit%20dev/Documentation/FIRMWARE_DOCS.md)                  |
| User Testing Guide            | [User_Testing_Guide.md](Central%20Control%20Unit%20dev/Documentation/User_Testing_Guide.md)        |
| WebApp Developer Docs         | [WEBAPP_DOCS.md](Smart-Outlet-WebApp%20dev/Documentation/WEBAPP_DOCS.md)                           |

---

## Team Notes / Discussion

> Use this section for ongoing decisions, open questions, or notes for the team.

<!-- Add notes below this line -->


