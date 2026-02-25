# 🔌 Smart Outlet System — Project Milestones

**Last Updated:** February 25, 2026

---

## System Architecture

![Smart Outlet System Architecture](Smart%20outlet%20system%20Architecture.png)

---

## Component Status

| Component                | Version | Status         | Documentation                                                                 |
|:-------------------------|:--------|:---------------|:------------------------------------------------------------------------------:|
| SmartOutlet Firmware     | v5.3.1  | ✅ Stable      | [FIRMWARE_DOCS.md](Smart%20Outlet%20Device%20dev/Documentation/FIRMWARE_DOCS.md) |
| CCU Firmware (ESP32)     | v4.0.0  | ✅ Stable      | [FIRMWARE_DOCS.md](Central%20Control%20Unit%20dev/Documentation/FIRMWARE_DOCS.md) |
| Smart-Outlet-WebApp      | v1.0.0  | ✅ Stable      | [WEBAPP_DOCS.md](Smart-Outlet-WebApp%20dev/Documentation/WEBAPP_DOCS.md)        |
| Outlet Breaker (SCT013)  | v4.0.0  | ✅ Stable      | [FIRMWARE_DOCS.md](Central%20Control%20Unit%20dev/Documentation/FIRMWARE_DOCS.md) |

---

## Milestone Log

### CCU Firmware (ESP32)

| Date       | Version | Milestone                                                                 |
|:-----------|:--------|:--------------------------------------------------------------------------|
| 2026-02-25 | v4.0.0  | Auto device registration — ESP32 syncs device list from Django on boot + periodic re-sync |
| 2026-02-25 | v4.0.0  | Relay state desync fix — default `-1` (unknown) correctly mapped to OFF   |
| 2026-02-25 | v4.0.0  | Dynamic overload reporting — `is_overload` flag no longer hardcoded       |
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
| 2026-02-25 | v1.0.0  | **first_stable_UI** — Full cloud integration with real-time dashboard     |
| 2026-02-25 | v1.0.0  | Auto device registration — ESP32 fetches device list from Django via `/api/devices/` |
| 2026-02-25 | v1.0.0  | Overload alert pipeline — `is_overload` flag + threshold exceeded alerts  |
| 2026-02-25 | v1.0.0  | Relay state sync fix — default states report correctly after ESP32 reboot |
| 2026-02-24 | —       | Cloud communication pipeline — sensor data, breaker data, command polling |
| 2026-02-24 | —       | JSON casing bug fix — `toUpperCase()` was corrupting keys                |
| 2026-02-23 | —       | WebSocket real-time UI — sensor + breaker data broadcast                  |
| 2026-02-23 | —       | Database migration fixes — `PendingCommand` table recreation              |
| 2026-02-23 | —       | Main dashboard UI — outlet cards, toggle switches, threshold controls     |
| 2026-02-16 | —       | Data Logs page — filterable event logs with statistics                    |
| 2026-02-12 | —       | Initial setup — Django project with dev server running                    |

---

## Roadmap

- [x] Outlet Breaker — SCT013-100A main load monitoring via ESP32
- [x] WebApp full cloud integration — ESP32 ↔ Django server data sync
- [x] Online Dashboard — CRUD for outlets, threshold config, relay controls
- [x] Auto device registration — ESP32 syncs outlets from Django on boot
- [x] Overload & threshold alert system — immediate alerts in database
- [ ] Auto cut-off — automatically kill all outlets when breaker threshold exceeded
- [ ] Persistent device storage on ESP32 (SPIFFS/NVS instead of RAM)
- [ ] OTA firmware updates for ESP32
- [ ] AI chatbot integration with outlet data
- [ ] Scheduling system — automatic relay ON/OFF by time

---

## Documentation Index

| Document                      | Location                                                                                          |
|:------------------------------|:--------------------------------------------------------------------------------------------------|
| System Architecture           | [Smart outlet system Architecture.png](Smart%20outlet%20system%20Architecture.png)                |
| PIC Firmware Docs             | [FIRMWARE_DOCS.md](Smart%20Outlet%20Device%20dev/Documentation/FIRMWARE_DOCS.md)                   |
| PIC Circuit Diagram           | [Smart outlet_Actual_Circuit_v5.2.0.PDF](Smart%20Outlet%20Device%20dev/Images/Smart%20outlet_Actual_Circuit_v5.2.0.PDF) |
| CCU Firmware Docs             | [FIRMWARE_DOCS.md](Central%20Control%20Unit%20dev/Documentation/FIRMWARE_DOCS.md)                  |
| User Testing Guide (CCU)      | [User_Testing_Guide.md](Central%20Control%20Unit%20dev/Documentation/User_Testing_Guide.md)        |
| Web App Developer Docs        | [WEBAPP_DOCS.md](Smart-Outlet-WebApp%20dev/Documentation/WEBAPP_DOCS.md)                           |
| Web App User Setup Guide      | [User_Setup_Guide.md](Smart-Outlet-WebApp%20dev/Documentation/User_Setup_Guide.md)                |

---

## Team Notes / Discussion

> Use this section for ongoing decisions, open questions, or notes for the team.

<!-- Add notes below this line -->


