# Smart-Outlet-WebApp — Developer Documentation

**Framework:** Django 5.2 · **ASGI Server:** Daphne · **Database:** PostgreSQL  
**Real-Time:** Django Channels (WebSockets) · **Version:** v7.0.0

---

## Quick Start Guide

### 1. Start the Local Server

All commands run from the project root: `C:\Users\USER\Documents\Smart-Outlet-WebApp`

```powershell
# Step 1: Activate the virtual environment
.\.venv\Scripts\Activate

# Step 2: Start the development server (supports WebSockets via Daphne ASGI)
.venv\Scripts\python.exe manage.py runserver 0.0.0.0:8000
```

Then open **http://localhost:8000** in your browser.

> **Note:** Using `0.0.0.0:8000` allows the ESP32 and other devices on the local network to reach the server. Django's `runserver` with Daphne handles both HTTP and WebSocket connections.

### 2. Find Your PC's IP Address

The ESP32 needs your PC's LAN IP to connect to the server.

```powershell
# Get your WiFi/Ethernet IP
(Get-NetIPAddress -AddressFamily IPv4 | Where-Object {$_.InterfaceAlias -like "*Wi-Fi*" -or $_.InterfaceAlias -like "*Ethernet*"}).IPAddress
```

Look for the one starting with `192.168.x.x` — that's your LAN IP.

### 3. Setup ESP32 WiFi

1. Power on the ESP32 — it starts in **AP mode** (`CCU-Setup` WiFi network)
2. Connect your phone/laptop to the `CCU-Setup` WiFi
3. A captive portal opens (or go to `http://192.168.4.1`)
4. Click **WiFi Settings** or go to `http://192.168.4.1/settings`
5. Enter:
   - **SSID:** Your home WiFi name
   - **Password:** Your WiFi password
   - **Server URL:** `http://<Your-PC-IP>:8000` (e.g. `http://192.168.1.192:8000`)
6. Click **Save** — ESP32 restarts and connects to your WiFi + server

### 4. Kill the Local Server

```powershell
# If you still have the terminal open:
# Press Ctrl+C

# If you accidentally closed the terminal:
taskkill /F /PID (Get-NetTCPConnection -LocalPort 8000 -ErrorAction SilentlyContinue).OwningProcess
```

### 5. Run Database Migrations

```powershell
# Generate new migration files
.\venv\Scripts\python.exe manage.py makemigrations outlets

# Apply migrations to Supabase
.\venv\Scripts\python.exe manage.py migrate
```

### 6. Common Commands

| Command | Description |
|:--------|:------------|
| `.\.venv\Scripts\Activate` | Activate the Python virtual environment |
| `.venv\Scripts\python.exe manage.py runserver 0.0.0.0:8000` | Start server (with WebSocket support via Daphne ASGI) |
| `.venv\Scripts\python.exe manage.py makemigrations outlets` | Generate migration files |
| `.venv\Scripts\python.exe manage.py migrate` | Apply migrations to database |
| `.venv\Scripts\python.exe manage.py createsuperuser` | Create admin user |
| `taskkill /F /PID (Get-NetTCPConnection -LocalPort 8000 -ErrorAction SilentlyContinue).OwningProcess` | Kill server on port 8000 |

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                     Django Backend                           │
│                                                              │
│  ┌──────────────┐   ┌──────────────┐   ┌───────────────┐    │
│  │ Django ORM   │   │ Django Views │   │ Django        │    │
│  │ PostgreSQL   │   │ REST API     │   │ Channels      │    │
│  └──────┬───────┘   └──────┬───────┘   └───────┬───────┘    │
│         │                  │                    │            │
│    Models (DB)        HTTP Endpoints       WebSocket         │
│    - Outlet           - /api/data/         - /ws/sensor/     │
│    - SensorData       - /api/breaker-data/ - /ws/breaker/    │
│    - PendingCommand   - /api/commands/                       │
│    - Alert            - /api/devices/                        │
│    - MainBreakerReading                                      │
│    - CentralControlUnit                                      │
│    - EventLog                                                │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │   Frontend  (templates/home.html)                      │  │
│  │   JavaScript fetch()  +  WebSocket  +  Bootstrap 5     │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────┬───────────────────────────────────┘
                           │  HTTP POST/GET (JSON)
                    ┌──────┴──────┐
                    │ ESP32 (CCU) │  ← Cloud.cpp HTTP Client
                    │             │     Sends sensor data
                    │             │     Fetches commands
                    │             │     Syncs device list
                    └──────┬──────┘
                           │  HC-12 433MHz RF
                    ┌──────┴──────┐
                    │ PIC16F88    │  Smart Outlet(s)
                    └─────────────┘
```

---

## Database Models

### Outlet

| Field        | Type        | Default  | Notes                              |
|:-------------|:------------|:---------|:-----------------------------------|
| `user`       | ForeignKey  | —        | Owner (Django `User`)              |
| `name`       | CharField   | —        | User-assigned label                |
| `device_id`  | CharField   | —        | Unique hex ID, e.g. `"FE"`        |
| `location`   | CharField   | `""`     | Optional location label            |
| `relay_a`    | BooleanField| `False`  | Socket A relay state               |
| `relay_b`    | BooleanField| `False`  | Socket B relay state               |
| `threshold`  | IntegerField| `0`      | Current threshold in mA            |
| `ccu`        | ForeignKey  | null     | Linked `CentralControlUnit` (auto-set) |
| `created_at` | DateTime    | auto     | Creation timestamp                 |
| `updated_at` | DateTime    | auto     | Last update timestamp              |

### SensorData

| Field        | Type         | Default  | Notes                              |
|:-------------|:-------------|:---------|:-----------------------------------|
| `outlet`     | ForeignKey   | —        | Linked `Outlet`                    |
| `current_a`  | IntegerField | `0`      | Socket A current in mA             |
| `current_b`  | IntegerField | `0`      | Socket B current in mA             |
| `is_overload`| BooleanField | `False`  | True if `0xFFFF` overload trip     |
| `timestamp`  | DateTime     | auto     | Reading timestamp                  |

> **DB Write Throttle:** Sensor data is only persisted to the database every **1 minute** (`DB_LOG_INTERVAL = 60s`). All data is broadcast via WebSocket immediately for real-time UI.

> **Noise Floor Filter:** Outlet sensor readings between 1–100 mA are clamped to `0` server-side before logging or broadcasting. This eliminates PIC baseline noise (common defaults: 49 mA, 98 mA).

### MainBreakerReading

| Field        | Type         | Default  | Notes                              |
|:-------------|:-------------|:---------|:-----------------------------------|
| `ccu_id`     | CharField    | —        | CCU sender ID, e.g. `"01"`         |
| `ccu_device` | ForeignKey   | null     | Linked `CentralControlUnit`        |
| `current_ma` | IntegerField | —        | Total load current in mA           |
| `timestamp`  | DateTime     | auto     | Reading timestamp                  |

> **Noise Floor Filter:** Main breaker readings between 1–250 mA are clamped to `0` server-side. This eliminates SCT-013 sensor noise when no actual load is present.

### PendingCommand

| Field        | Type         | Default  | Notes                              |
|:-------------|:-------------|:---------|:-----------------------------------|
| `outlet`     | ForeignKey   | —        | Target `Outlet`                    |
| `command`    | CharField    | —        | `relay_on`, `relay_off`, `set_threshold`, `read_sensors`, `ping` |
| `socket`     | CharField    | `""`     | `"a"`, `"b"`, or `""` for device-level |
| `value`      | IntegerField | null     | For threshold values (mA)          |
| `is_executed`| BooleanField | `False`  | Marked `True` after CCU fetches it |
| `created_at` | DateTime     | auto     | Queue timestamp                    |

### Alert

| Field        | Type         | Default  | Notes                              |
|:-------------|:-------------|:---------|:-----------------------------------|
| `outlet`     | ForeignKey   | —        | Source `Outlet`                    |
| `alert_type` | CharField    | —        | `overload`, `threshold`, `offline` |
| `message`    | TextField    | —        | Human-readable alert message       |
| `is_read`    | BooleanField | `False`  | Dismissal flag                     |
| `created_at` | DateTime     | auto     | Alert timestamp                    |

### CentralControlUnit

| Field        | Type         | Default    | Notes                           |
|:-------------|:-------------|:-----------|:--------------------------------|
| `user`       | ForeignKey   | —          | Owner (Django `User`)           |
| `ccu_id`     | CharField    | —          | Unique CCU ID, e.g. `"01"`     |
| `name`       | CharField    | `"My CCU"` | User-assigned label             |
| `location`   | CharField    | `""`       | Optional location               |
| `ip_address` | GenericIP    | null       | ESP32's LAN IP (auto-captured)  |
| `last_seen`  | DateTime     | null       | Last data push timestamp        |
| `focused_device` | CharField | `""`       | Currently expanded outlet (hex) |
| `created_at` | DateTime     | auto       | Registration timestamp          |

> **IP Capture:** The ESP32's IP is automatically captured from every `/api/data/` and `/api/breaker-data/` POST. This enables direct communication.

### EventLog

Audit log for tracking user actions and system events.

| Field        | Type         | Default  | Notes                              |
|:-------------|:-------------|:---------|:-----------------------------------|
| `user`       | ForeignKey   | null     | User who triggered the action      |
| `source`     | CharField    | —        | `WEB_DASHBOARD`, `PIC_HARDWARE`, `SERVER` |
| `action_type`| CharField    | —        | `TOGGLE_RELAY`, `OVERLOAD_TRIPPED`, `SET_THRESHOLD`, `THRESHOLD_EXCEEDED` |
| `target_device` | CharField | `""`     | e.g. `0xFE`, `All Devices`         |
| `details`    | TextField    | `""`     | Human-readable description         |
| `created_at` | DateTime     | auto     | Event timestamp                    |

---

## REST API Reference

### ESP32 → Django (Data Ingestion)

| Method | Route                         | Function                | Payload / Notes                     |
|:-------|:------------------------------|:------------------------|:------------------------------------|
| POST   | `/api/data/`                  | `receive_sensor_data`   | `{device_id, current_a, current_b, relay_a, relay_b, is_overload}` |
| POST   | `/api/breaker-data/`          | `receive_breaker_data`  | `{ccu_id, current_ma}`              |

### ESP32 → Django (Command Polling)

| Method | Route                         | Function                | Response                            |
|:-------|:------------------------------|:------------------------|:------------------------------------|
| GET    | `/api/commands/<device_id>/`  | `get_pending_commands`  | `{success, commands: [{command, socket, value}]}` |
| GET    | `/api/devices/`               | `get_registered_outlets`| `{success, devices: ["FE", "FD"]}` |

### Focus Device (Expand/Collapse)

| Method | Route                         | Function                | Notes                               |
|:-------|:------------------------------|:------------------------|:------------------------------------|
| GET    | `/api/focus/`                 | `get_focus_device`      | Returns `{success, device_id}` — ESP32 polls this |
| POST   | `/api/focus/<device_id>/`     | `set_focus_device`      | Sets focused device (expand)        |
| POST   | `/api/focus/clear/`           | `clear_focus_device`    | Clears focus (collapse)             |

### UI → Django (User Actions)

| Method | Route                                    | Function           | POST Params                    |
|:-------|:-----------------------------------------|:-------------------|:-------------------------------|
| POST   | `/api/commands/<device_id>/<command>/`    | `queue_command`    | `socket`, `value` (optional)   |
| GET    | `/api/outlet-status/<device_id>/`        | `get_outlet_status`| —                              |

### Valid Commands

| Command          | Socket Required | Value Required | Action                          |
|:-----------------|:----------------|:---------------|:--------------------------------|
| `relay_on`       | Yes (`a`/`b`)   | No             | Turn relay ON                   |
| `relay_off`      | Yes (`a`/`b`)   | No             | Turn relay OFF                  |
| `set_threshold`  | No              | Yes (mA)       | Set overload threshold          |
| `read_sensors`   | No              | No             | Request fresh sensor reading    |
| `ping`           | No              | No             | Ping the PIC device             |

---

## Data Flow Pipeline

### Sensor Data (PIC → Cloud UI)

```
1. ESP32 sends [TX] Read Sensors → 0xFE  (HC-12 RF)
2. PIC replies with DATA REPORT (current mA for Socket A & B)
3. ESP32 parses response, builds JSON payload
4. ESP32 POSTs to /api/data/
5. Django:
   a. Updates Outlet.relay_a / relay_b in DB (always)
   b. Checks for overload alerts (always)
   c. Broadcasts via WebSocket (always)
   d. Saves to SensorData table (every 5 min only)
6. Browser receives WebSocket message → updates UI in real-time
```

### Commands — Direct Communication (UI → ESP32 → PIC)

```
1. User clicks toggle switch on web UI
2. JavaScript fetch() POSTs to /api/commands/FE/relay_on/
3. Django looks up ESP32 IP from CentralControlUnit.ip_address
4. Django sends HTTP POST to http://<ESP32_IP>/api/ext/relay
5. ESP32 sends HC-12 RF packet to PIC
6. PIC toggles relay, sends CMD_ACK back
7. ESP32 returns JSON {success: true, ack: true} to Django
8. Django updates Outlet state in DB + creates EventLog
9. UI toast: "✅ Relay ON confirmed by ESP32"
```

### Commands — Fallback (Polling Queue)

```
If ESP32 is unreachable (offline, wrong IP, timeout):
1. Django falls back to creating a PendingCommand record
2. ESP32 polls GET /api/commands/FE/ (every 2s)
3. Django returns pending commands, marks them as executed
4. ESP32 parses command, sends HC-12 RF packet to PIC
5. UI toast: "⚠️ Command queued (ESP32 offline)"
```

### Auto Device Registration (on boot)

```
1. ESP32 boots → connects to WiFi
2. ESP32 GETs /api/devices/
3. Django returns ["FE", "FD"]
4. ESP32 registers each device in OutletManager
5. Normal polling loop starts — no manual d FE needed
```

---

## WebSocket Channels

| Route                      | Consumer          | Group Pattern     | Data Format              |
|:---------------------------|:-------------------|:-----------------|:-------------------------|
| `/ws/sensor/<device_id>/`  | `SensorConsumer`   | `sensor_FE`      | `{type, data: {device_id, current_a, current_b, relay_a, relay_b, is_overload}}` |
| `/ws/breaker/<ccu_id>/`    | `BreakerConsumer`  | `breaker_01`     | `{type, data: {ccu_id, current_amps}}` |

WebSocket connections auto-reconnect after 5 seconds on disconnect.

---

## Alert System

Alerts fire **immediately** (not subject to the 5-minute DB throttle):

| Alert Type  | Trigger Condition                                        |
|:------------|:---------------------------------------------------------|
| `overload`  | `is_overload == True` or `current_a/b == 65535 (0xFFFF)` |
| `threshold` | `current_a > outlet.threshold` or `current_b > outlet.threshold` |
| `offline`   | Reserved for future device heartbeat monitoring          |

---

## Frontend (home.html)

### JavaScript Functions

| Function                     | Description                                    |
|:-----------------------------|:-----------------------------------------------|
| `sendCommand(deviceId, cmd, socket, value)` | Generic command sender via `fetch()` POST. Returns `{success, direct}` |
| `toggleRelay(deviceId, socket, state)`      | Toggle relay — adds loading state (dim + disable), reverts on failure |
| `setOutletThreshold(deviceId)`              | Set per-outlet threshold from input      |
| `setMainThreshold()`                        | Set main breaker threshold               |
| `cutAllPower()`                             | Emergency: turn OFF all relays on all outlets |
| `connectWebSocket(deviceId, isBreaker)`     | Open WebSocket + handle real-time updates |
| `showToast(message, type)`                  | Display success/error notification       |

### Real-Time UI Updates

When a WebSocket message arrives:
- **Sensor data:** Updates Socket A/B current values, relay toggle states, overload indicators, active/inactive badges, and breaker panel outlet list
- **Breaker data:** Updates the total load current display with color-coded status (green/yellow/red)
- The UI reflects state changes within ~1-2 seconds of the physical event

### Breaker Monitor Panel

The Total Load Current card is **expandable** — click to reveal:
- **Color-coded status:** Green (normal), Yellow (near limit ≥80%), Red (overload ≥100%) based on threshold
- **Active outlet list:** Shows each outlet's live current with individual **Cut** button
- **Cut All Power** button to kill all relay outputs at once
- **Threshold config** — set breaker limit in mA

### Event History Page (`event_history.html`)

Accessible from the profile dropdown → **Event History**. Displays the last 100 events from `EventLog`.

| Feature | Description |
|:--------|:------------|
| **Filter buttons** | All, Overload, Threshold, Relay, Server |
| **View modes** | Cards (default) or Table view |
| **Color-coded icons** | Red (overload), Yellow (threshold), Green (relay), Purple (server) |
| **Source badges** | Web / PIC / Server labels |
| **Device badge** | Shows target device ID (e.g., `0xFE`) |
| **Details** | Socket info, current readings, etc. |

---

## Project File Structure

```
Smart-Outlet-WebApp/
├── manage.py                    — Django management script
├── requirements.txt             — Python dependencies
├── .env                         — Environment variables (SECRET_KEY, DB config)
│
├── config/                      — Django project settings
│   ├── settings.py              — Main settings (DATABASES, CHANNEL_LAYERS, etc.)
│   ├── urls.py                  — Root URL configuration
│   ├── asgi.py                  — ASGI application (Daphne + Channels)
│   └── wsgi.py                  — WSGI fallback
│
├── outlets/                     — Main app: outlet models, views, admin
│   ├── models.py                — Outlet, SensorData, Alert, PendingCommand, etc.
│   ├── views.py                 — Page views (home, login, register)
│   ├── urls.py                  — Outlet URL patterns
│   ├── consumers.py             — WebSocket consumers (SensorConsumer, BreakerConsumer)
│   ├── routing.py               — WebSocket URL routing
│   └── admin.py                 — Django admin registrations
│
├── api/                         — REST API for ESP32 communication
│   ├── views.py                 — API endpoints (receive_sensor_data, queue_command, etc.)
│   └── urls.py                  — API URL patterns (/api/...)
│
├── chatbot/                     — AI chatbot module
│
├── templates/                   — HTML templates
│   ├── home.html                — Main dashboard (outlets, controls, WebSocket)
│   ├── event_history.html       — Event History (filterable, card/table views)
│   ├── login.html               — Login page
│   ├── register.html            — Registration page
│   └── ...
│
├── static/                      — Static files (CSS, JS, images)
│
├── Central_Control_Unit_Firmware/ — ESP32 firmware (Arduino C++)
│
└── Milestone/                   — Project documentation & milestones
```

---

## Configuration Constants

### Django Settings

| Setting              | Value                    | Notes                           |
|:---------------------|:-------------------------|:--------------------------------|
| `DB_LOG_INTERVAL`    | `60 seconds`             | Min interval between DB writes (sensor data) |
| `BREAKER_LOG_INTERVAL` | `5 minutes`            | Min interval between DB writes (breaker data) |
| `NOISE_FLOOR_MA`     | `100`                    | Outlet readings 1–100 mA → 0   |
| `BREAKER_NOISE_FLOOR_MA` | `250`                | Breaker readings 1–250 mA → 0  |
| `CLOUD_SEND_INTERVAL_MS` | `2000` (firmware)    | ESP32 polling interval          |
| `HTTP_TIMEOUT_MS`    | `5000` (firmware)        | HTTP request timeout            |

### ESP32 Server URL Format

```
http://<Django_Server_IP>:8000
```

Example: `http://10.31.253.107:8000`

> The ESP32 must be on the same network as the Django server. The server should be started with `python manage.py runserver 0.0.0.0:8000` to accept connections from any local IP.

---

## Known Behaviors & Edge Cases

| Behavior | Description |
|:---------|:------------|
| **Relay state desync on reboot** | When the ESP32 reboots, relay states default to `-1` (unknown). The firmware sends `relay_a: false` until an ACK confirms the actual state. |
| **Threshold desync** | The PIC's threshold is set via HC-12 RF. Django stores the threshold separately. Both must match for alert detection to work. |
| **Auto device sync** | The ESP32 re-fetches `/api/devices/` every 2 seconds. New outlets added on the web UI are detected within 2 seconds — no reboot required. |
| **WebSocket reconnect** | If the WebSocket connection drops, the frontend auto-reconnects after 5 seconds. |
| **DB write throttle** | `SensorData` is only saved every 5 minutes. Real-time data is always available via WebSocket. |
| **Direct fallback** | If ESP32 is unreachable for direct HTTP, Django silently falls back to `PendingCommand` queue (~2s delay). |
| **Stale IP** | ESP32 IP is updated on every sensor push. If the IP changes, the next push auto-corrects it. |
| **Current display** | Current values are displayed in milliamperes (mA) without rounding for higher precision. |
| **Noise floor** | Outlet readings 1-100mA and breaker readings 1-250mA are clamped to 0 server-side to filter sensor noise. |
| **Focus Device** | Only the expanded outlet receives sensor reads. Collapsed outlets show last known values with disabled toggles. ESP32 polls `/api/focus/` every 2s. |
| **Last-write-wins** | If two users expand different outlets, the last expansion wins. All users see the same focused device. |
