# 🔌 Smart Outlet System — Project Milestones

**Last Updated:** March 7, 2026 (v9.0.0 — Development Roadmap Update)

---

## System Architecture

![Smart Outlet System Architecture](Smart%20outlet%20system%20Architecture.png)

---

## Component Status

| Component                | Version | Status         | Documentation                                                                 |
|:-------------------------|:--------|:---------------|:------------------------------------------------------------------------------|
| SmartOutlet Firmware     | v5.3.1  | ✅ Stable      | [FIRMWARE_DOCS.md](Smart%20Outlet%20Device%20dev/Documentation/FIRMWARE_DOCS.md) |
| CCU Firmware (ESP32)     | v8.1.0  | ✅ Stable      | [FIRMWARE_DOCS.md](Central%20Control%20Unit%20dev/Documentation/FIRMWARE_DOCS.md) |
| Smart-Outlet-WebApp      | v8.1.0  | ✅ Stable      | [WEBAPP_DOCS.md](Smart-Outlet-WebApp%20dev/Documentation/WEBAPP_DOCS.md)            |
| Outlet Breaker (SCT013)  | v8.1.0  | ✅ Stable      | [FIRMWARE_DOCS.md](Central%20Control%20Unit%20dev/Documentation/FIRMWARE_DOCS.md) |

---

## Milestone Log

### CCU Firmware (ESP32)

| Date       | Version | Milestone                                                                 |
|:-----------|:--------|:--------------------------------------------------------------------------|
| 2026-03-05 | v8.1.0  | Live breaker line chart, outlet card state caching, breaker noise floor 250→280 mA |
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

## Completed Milestones

- [x] Outlet Breaker — SCT013-100A main load monitoring via ESP32
- [x] WebApp full cloud integration — ESP32 ↔ Django server data sync
- [x] Direct ESP32 communication — relay commands via HTTP (bypasses polling queue)
- [x] Focus Device — only read/control the expanded outlet (mirrors local dashboard)
- [x] Expandable breaker monitor — color-coded load card, per-outlet cut buttons
- [x] Noise floor filters — clamp sensor noise to 0 (PIC: 0-100mA, SCT013: 0-280mA)
- [x] Event History page — filterable event log with card/table views
- [x] Real-time status badges — Active/Inactive counts update live via WebSocket
- [x] Online Dashboard — CRUD for outlets, threshold config, AI chat panel
- [x] Live breaker line chart (Chart.js) + card state caching
- [x] Breaker threshold persistence — saved to CCU model, stamps each reading for spreadsheet
- [x] Google Sheets integration — auto-export sensor data

---

## Development Roadmap v9.0.0

| Priority | Feature | Complexity | Risk | Status |
|:--------:|:--------|:----------:|:----:|:------:|
| 1 | Smart Outlet LED Activation | Low | Low | [ ] |
| 2 | AI Chatbot Integration | Medium | Low | [ ] |
| 3 | Notifications UI | Medium | Low | [ ] |
| 4 | Host to Render (Cloud Deployment) | Medium | Medium | [ ] |
| 5 | Temporary ESP Data Local Storage | High | Medium | [ ] |
| 6 | Auto Cut-Off on Near-Threshold | High | **High** | [ ] |

### Priority 1: Smart Outlet LED Activation
The smart outlet has a built-in LED indicator that turns HIGH when the master ID, device ID, and threshold are configured out of their defaults. Since the CCU currently uses ID `001` — the default known master ID — the LED remains LOW. Fix by configuring the CCU ID to a non-default value or updating the firmware logic.

### Priority 2: AI Chatbot Integration (Gemini Function Calling)
Integrate the existing chatbot UI with Google Gemini using **function calling** so users can control outlets, query socket readings, and get analytics through natural language — supports English, Filipino, and Bisaya.

### Priority 3: Notifications UI
Build the notification bell icon on the homepage. Users receive real-time notifications from AI analytics, system events (overload, auto-cutoff), and threshold warnings. Requires a `Notification` model and WebSocket push delivery.

### Priority 4: Host to Render (Cloud Deployment)
Deploy the Django + Daphne ASGI app to Render. Requires migration from SQLite to **PostgreSQL**, **Redis** for Channels layer, `whitenoise` for static files, and updating the ESP32 firmware to send data to the public Render URL instead of LAN IP.

### Priority 5: Temporary ESP Data Local Storage
Remove the AP mode fallback on WiFi disconnect. Instead, auto-reconnect to saved credentials and temporarily buffer sensor data in ESP32 local storage (SPIFFS/LittleFS) during offline periods. Auto-sync buffered data to the cloud database on reconnection, then purge local copies.

> ⚠️ **Caution:** "Huge APP" partition scheme leaves limited SPIFFS space (~128-256KB). Use compact binary format and RAM buffering before flash writes to reduce wear.

### Priority 6: Auto Cut-Off on Near-Threshold (Safety Critical)
Both online and offline, the ESP32 must auto-cut the socket drawing the highest current when the breaker reading approaches the declared threshold. Must log the event and notify the user. Requires per-socket current knowledge, filtered readings to avoid false positives, configurable trigger percentage, and a cooldown period before re-enabling.

> ⚠️ **Caution:** This is the highest-risk feature. Auto-cutting appliances (computers, refrigerators) could cause damage. Consider a "protected socket" flag that is exempt from auto-cutoff.

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

### 📋 Upcoming: v9.0.0 — OTA Firmware Updates + Role-Based Access (March 2026)

**Phase 1 — Optimize CCU Firmware (reduce flash for OTA-compatible partition)**
- [ ] Extract inline HTML/CSS/JS from `Dashboard.cpp` (510 lines) and `CaptivePortal.cpp` (278 lines) into SPIFFS files
- [ ] Deduplicate shared CSS, minify HTML
- [ ] Switch partition: Huge APP (3MB) → Minimal SPIFFS (1.9MB APP + OTA + 190KB SPIFFS)

**Phase 2 — Enable OTA on ESP32**
- [ ] Add Web OTA upload page (`/update` route on Dashboard)
- [ ] Add Cloud OTA — ESP32 polls Django server for firmware updates
- [ ] Add `FIRMWARE_VERSION` constant to Config.h

**Phase 3 — Role-Based Access Control (Django WebApp)**
- [ ] Three roles: Normal User, Admin, Developer
- [ ] Admin can view accounts and approve developer role requests
- [ ] Users can request developer access via feedback form

**Phase 4 — Firmware Management Page (Django WebApp)**
- [ ] Developers can upload `.bin` firmware as Draft, then Publish when ready
- [ ] Normal users can view published patch versions and download
- [ ] ESP32 checks `/api/firmware/check/` for available updates

---

### 📝 Detailed Implementation Plan: OTA Firmware Updates + Role-Based Access Control

Full implementation plan for enabling over-the-air firmware updates and adding developer/admin/user roles to the Smart Outlet system.

---

#### Phase 1: Optimize CCU Firmware (Reduce Flash Usage)

> **Goal:** Shrink the firmware binary from ~1.14 MB to ~800-900 KB so it fits in an OTA-compatible partition.

**Why this is first**
OTA requires two APP partitions (current + incoming). The current **Huge APP (3MB)** partition has no OTA slot. We need to shrink the firmware to fit in **Minimal SPIFFS (1.9MB APP + OTA + 190KB SPIFFS)** or **No OTA (2MB APP + 2MB SPIFFS)** as a stepping stone.

**The problem**
`Dashboard.cpp` is **53.5 KB** — 510 lines of inline HTML/CSS/JS in `_buildDashboardPage()` (lines 598–1108) and 86 lines in `_buildSettingsPage()` (lines 1114–1199). `CaptivePortal.cpp` is **17.9 KB** with similar inline content. Together they account for ~71 KB of source that compiles into large string constants in flash.

**Changes**

**1a. Extract HTML/CSS/JS to SPIFFS files**

| [NEW] File on SPIFFS | Extracted from | Est. size |
|:---------------------|:---------------|:----------|
| `/dashboard.html` | `Dashboard::_buildDashboardPage()` | ~35 KB |
| `/settings.html` | `Dashboard::_buildSettingsPage()` | ~6 KB |
| `/setup.html` | `CaptivePortal` setup page HTML | ~12 KB |
| `/style.css` | Shared CSS (deduplicated from all pages) | ~4 KB |

**[MODIFY] Dashboard.cpp**
- Replace `_buildDashboardPage()` — instead of returning a giant `String`, read `/dashboard.html` from SPIFFS and serve it
- Replace `_buildSettingsPage()` — same approach, read `/settings.html` from SPIFFS
- Add a helper `_serveFile(path)` to read SPIFFS files and inject dynamic values (SSID, server URL, etc.) via simple placeholder replacement (e.g., `{{SSID}}` → actual value)

**[MODIFY] CaptivePortal.cpp**
- Extract the setup page HTML into `/setup.html` on SPIFFS
- Serve via the same `_serveFile()` helper

**1b. Minify + gzip the HTML files**
- Strip whitespace and comments from the extracted `.html` files
- Optionally gzip-compress them (ESP32 WebServer supports `Content-Encoding: gzip`)
- Expected savings: 60-70% reduction in file sizes

**1c. Switch partition scheme**
- Change from **Huge APP (3MB No OTA / 1MB SPIFFS)** to **Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**
- The extracted HTML files (~57 KB total, ~20 KB gzipped) fit comfortably in 190 KB SPIFFS
- Upload the HTML files to SPIFFS using Arduino IDE's **ESP32 Sketch Data Upload** tool

> ⚠️ After switching partitions, the firmware binary must be under **1.9 MB**. Current size is ~1.14 MB minus the removed inline strings (~30-40 KB savings) = ~1.1 MB — well within limits.

---

#### Phase 2: Enable OTA on ESP32

> **Goal:** ESP32 can receive firmware updates over WiFi — both locally and from the Django cloud server.

**2a. Web OTA (Local WiFi upload page)**

**[MODIFY] Dashboard.h**
- Add `_handleOTAPage()` and `_handleOTAUpload()` method declarations

**[MODIFY] Dashboard.cpp**
- Register routes: `GET /update` (upload form) and `POST /update` (receives .bin file)
- Use the ESP32 `Update` library (`#include <Update.h>`) to flash incoming binary
- Add a simple HTML form for file upload (stored in SPIFFS as `/update.html`)
- After successful flash → reboot automatically

**2b. Cloud OTA (Django-triggered updates)**

**[MODIFY] Cloud.cpp**
- Add a firmware version check to the existing polling loop
- Piggyback on the existing `/api/data/` POST — include `"firmware_version": "v8.1.0"` in the payload
- If Django responds with `"update_available": true` and `"update_url": "/api/firmware/download/..."` → trigger OTA download
- Download the `.bin` via HTTP, stream it through `Update.write()`, reboot on success

**[MODIFY] Config.h**
- Add `#define FIRMWARE_VERSION "v8.1.0"` — bumped with each release

> ⚠️ OTA has no rollback by default. If a bad firmware is flashed, the ESP32 could become unresponsive. Mitigation: keep the factory reset (GPIO 0 hold) working so users can re-enter setup mode and re-flash via USB as a last resort.

---

#### Phase 3: Role-Based Access Control (Django WebApp)

> **Goal:** Three user roles — Normal User, Admin, Developer — with a developer request/approval workflow.

**Roles and permissions**

| Permission | 👤 User | 🔧 Admin | 💻 Developer |
|:-----------|:--------|:---------|:-------------|
| Dashboard, relays, history | ✅ | ✅ | ✅ |
| View published patch list | ✅ | ✅ | ✅ |
| Download published firmware | ✅ | ✅ | ✅ |
| Request developer access | ✅ | — | — |
| View all registered accounts | ❌ | ✅ | ❌ |
| Approve/deny role requests | ❌ | ✅ | ❌ |
| Change user roles | ❌ | ✅ | ❌ |
| Upload firmware binaries | ❌ | ❌ | ✅ |
| Publish/deprecate patches | ❌ | ❌ | ✅ |
| See other developers | ❌ | ❌ | ✅ |
| Push OTA to connected CCUs | ❌ | ❌ | ✅ |

**Models**

**[MODIFY] User model (extend existing)**
- Add `role` field: `CharField(choices=["user", "admin", "developer"], default="user")`

**[NEW] `RoleRequest` model**
```
RoleRequest
├── user           → ForeignKey(User)
├── message        → TextField  ("Why I need dev access")
├── status         → CharField  ("pending" | "approved" | "denied")
├── reviewed_by    → ForeignKey(User, null)  (Admin who responded)
├── created_at     → DateTimeField(auto_now_add)
├── reviewed_at    → DateTimeField(null)
```

**Pages**

**[NEW] Admin Accounts Page (`/admin/accounts/`)**
- Table of all registered users: name, email, role, join date
- Pending role requests section with Approve/Deny buttons
- Ability to change any user's role directly

**[NEW] Developer Request Button (on user's profile or patch list page)**
- "Request Developer Access" button → modal with message field
- Creates a `RoleRequest` with `status="pending"`

**Security**
- `@role_required("admin")` decorator for admin-only pages
- `@role_required("developer")` decorator for developer-only pages
- Nav bar items conditionally rendered based on `request.user.role`

---

#### Phase 4: Firmware Management Page (Django WebApp)

> **Goal:** Developers can upload, publish, and deprecate firmware. Normal users can view published patches and download.

**Models**

**[NEW] `FirmwareVersion` model**
```
FirmwareVersion
├── version        → CharField(unique)    ("v9.0.0")
├── binary         → FileField            (.bin upload)
├── release_notes  → TextField            ("Auto cut-off, bug fixes")
├── status         → CharField            ("draft" | "published" | "deprecated")
├── uploaded_by    → ForeignKey(User)     (Developer who uploaded)
├── published_by   → ForeignKey(User, null) (Developer who published)
├── created_at     → DateTimeField(auto_now_add)
├── published_at   → DateTimeField(null)
```

**Pages**

**[NEW] Developer Firmware Page (`/developer/firmware/`)**
- **Upload form:** version, .bin file, release notes → creates as `status="draft"`
- **Patch table:** all versions with status badges (📝 Draft, ✅ Published, ⚫ Deprecated)
- **Actions:** Publish (draft→published), Deprecate (published→deprecated), Delete (deprecated only)
- **Developers list:** shows all users with `role="developer"`

**[NEW] User Patch List Page (`/firmware/`)**
- Lists only `status="published"` firmware versions
- Shows: version, release date, release notes, download button
- Shows the user's current CCU firmware version (if connected)

**API Endpoints**

**[NEW] `GET /api/firmware/check/`**
- Called by ESP32 during polling
- Params: `?current=v8.1.0`
- Returns: `{"update_available": true/false, "latest": "v9.0.0", "download_url": "..."}`

**[NEW] `GET /api/firmware/download/<version>/`**
- Serves the `.bin` file for the requested version
- Only serves `status="published"` versions

---

#### Verification Plan

**Phase 1 — Firmware Optimization**
- Compile firmware before and after SPIFFS extraction → compare binary sizes
- Verify all pages load correctly from SPIFFS (Dashboard, Settings, Setup)
- Confirm dynamic values (SSID, server URL) still inject properly

**Phase 2 — OTA**
- Test Web OTA: upload a .bin via `/update` page → verify ESP32 reboots with new firmware
- Test Cloud OTA: flag an update on Django → verify ESP32 auto-downloads and flashes
- Test rollback: verify factory reset (GPIO 0 hold) still works after OTA

**Phase 3 — RBAC**
- Test role assignment: user requests → admin approves → user becomes developer
- Test page access: verify users can't access admin/developer pages
- Test nav bar: verify menu items match the user's role

**Phase 4 — Firmware Management**
- Test upload: developer uploads .bin as draft → it's not visible to users
- Test publish: developer publishes → users can see and download
- Test deprecate: deprecated versions disappear from user list
- Test ESP32 check: verify `/api/firmware/check/` returns correct update info
