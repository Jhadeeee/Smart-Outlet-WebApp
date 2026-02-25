# Smart-Outlet-WebApp — Developer Documentation

**Framework:** Django 5.2 · **ASGI Server:** Daphne · **Database:** PostgreSQL  
**Real-Time:** Django Channels (WebSockets) · **Version:** v1.0.0 (first_stable_UI)

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

> **DB Write Throttle:** Sensor data is only persisted to the database every **5 minutes** (`DB_LOG_INTERVAL`). All data is broadcast via WebSocket immediately for real-time UI.

### MainBreakerReading

| Field        | Type         | Default  | Notes                              |
|:-------------|:-------------|:---------|:-----------------------------------|
| `ccu_id`     | CharField    | —        | CCU sender ID, e.g. `"01"`         |
| `ccu_device` | ForeignKey   | null     | Linked `CentralControlUnit`        |
| `current_ma` | IntegerField | —        | Total load current in mA           |
| `timestamp`  | DateTime     | auto     | Reading timestamp                  |

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
| `created_at` | DateTime     | auto       | Registration timestamp          |

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

### Commands (UI → Physical Relay)

```
1. User clicks toggle switch on web UI
2. JavaScript fetch() POSTs to /api/commands/FE/relay_on/
3. Django creates PendingCommand record in DB
4. Django speculatively updates Outlet.relay_a in DB for instant UI feedback
5. ESP32 polls GET /api/commands/FE/ (every 2s)
6. Django returns pending commands, marks them as executed
7. ESP32 parses command, sends HC-12 RF packet to PIC
8. PIC toggles relay, sends CMD_ACK back
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
| `sendCommand(deviceId, cmd, socket, value)` | Generic command sender via `fetch()` POST |
| `toggleRelay(deviceId, socket, state)`      | Toggle relay ON/OFF                      |
| `setOutletThreshold(deviceId)`              | Set per-outlet threshold from input      |
| `setMainThreshold()`                        | Set main breaker threshold               |
| `cutAllPower()`                             | Emergency: turn OFF all relays on all outlets |
| `connectWebSocket(deviceId, isBreaker)`     | Open WebSocket + handle real-time updates |
| `showToast(message, type)`                  | Display success/error notification       |

### Real-Time UI Updates

When a WebSocket message arrives:
- **Sensor data:** Updates Socket A/B current values, relay toggle states, overload indicators
- **Breaker data:** Updates the total load current display
- The UI reflects state changes within ~1-2 seconds of the physical event

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
| `DB_LOG_INTERVAL`    | `5 minutes`              | Min interval between DB writes  |
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
