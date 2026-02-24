# Smart-Outlet-WebApp — Developer Documentation

**Framework:** Django 5.2 · **Language:** Python 3.x · **Server:** Daphne (ASGI)  
**Database:** PostgreSQL (Supabase) / SQLite · **Real-time:** Channels + Redis  
**AI Integration:** Google Gemini API

---

## Architecture Overview

```text
┌────────────────────────────────────────────────────────────┐
│                    Smart-Outlet-WebApp                     │
│                                                            │
│  ┌──────────────┐   ┌──────────────┐   ┌───────────────┐  │
│  │   Auth App   │   │ Outlets App  │   │ Chatbot App   │  │
│  │ login/signup │   │ Web UI Pages │   │ Gemini Client │  │
│  └──────┬───────┘   └──────┬───────┘   └───────┬───────┘  │
│         │                  │                    │          │
│         └──────────────────┼────────────────────┘          │
│                            │                               │
│  ┌─────────────────────────▼──────────────────────────┐    │
│  │                Django URL Router                   │    │
│  │        (Routes requests to correct views)          │    │
│  └─────┬────────────┬─────────────────┬───────────┘    │
│        │            │                 │                │
│  ┌─────▼─────┐ ┌────▼──────┐  ┌──────▼──────┐         │
│  │ API Endpts│ │ Templates │  │ WebSockets  │         │
│  │ (ESP32)   │ │ (HTML/JS) │  │ (Channels)  │         │
│  └─────┬─────┘ └───────────┘  └──────┬──────┘         │
│        │                             │                 │
│  ┌─────▼─────────────────────────────▼─────────────┐   │
│  │                  Django ORM                     │   │
│  │          (Models: Outlet, SensorData)           │   │
│  └──────────────────────┬──────────────────────────┘   │
│                         │                              │
│  ┌──────────────────────▼──────────────────────────┐   │
│  │     PostgreSQL Database (Supabase) / SQLite     │   │
│  └─────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────┘
```

---

## Request Flow

The web app handles two main types of requests:

1. **HTTP Requests (Standard Web Traffic & API):**
   - **Client** requests a URL (e.g., `/dashboard/`).
   - **`config.urls`** routes it to `outlets.urls`.
   - **`views.dashboard`** fetches `Outlet` data via the ORM.
   - **Template** (`index.html`) is rendered with the data and sent back.

2. **WebSocket Connections (Real-time updates):**
   - **Client** connects via JavaScript to `ws://.../sensor/<device_id>/`.
   - **Daphne (ASGI)** routes the connection to `consumers.py`.
   - When ESP32 sends a POST to `/api/sensor-data/`, the view sends a message to the Channel Layer (Redis).
   - The Consumer pushes the live update down the WebSocket to the browser.

---

## Database Models

The app uses 5 primary models defined in `outlets/models.py`.

### `UserProfile` (Extended User Info)
Linked 1-to-1 with Django's built-in `User` model.
| Field          | Type         | Notes                          |
|:---------------|:-------------|:-------------------------------|
| `user`         | OneToOne     | Link to standard User model    |
| `country`      | Char(100)    | Optional                       |
| `barangay`     | Char(100)    | Optional                       |
| `phone_number` | Char(20)     | Optional                       |
| `address`      | TextField    | Optional                       |

### `Outlet` (Device Tracking)
Represents a single Smart Outlet (PIC) registered to a user.
| Field          | Type         | Notes                          |
|:---------------|:-------------|:-------------------------------|
| `user`         | ForeignKey   | Owner of the outlet            |
| `name`         | Char(100)    | Custom label (e.g., "Fan")     |
| `device_id`    | Char(50)     | Hex ID matching PIC (e.g., "FE") |
| `is_active`    | Boolean      | Current ON/OFF state           |

### `SensorData` (Telemetry)
Stores historical energy readings.
| Field          | Type         | Notes                          |
|:---------------|:-------------|:-------------------------------|
| `outlet`       | ForeignKey   | Link to specific Outlet        |
| `voltage`      | Float        | e.g., 230.5 V                  |
| `current`      | Float        | e.g., 2.3 A                    |
| `power`        | Float        | e.g., 500.5 W                  |
| `energy`       | Float        | Cumulative kWh                 |
| `temperature`  | Float        | (Optional) Celsius             |
| `timestamp`    | DateTime     | Auto-generated                 |

---

## URL Route Map

### Outlets App (`/`)
| Method | Route                   | View               | Purpose                                      |
|:-------|:------------------------|:-------------------|:---------------------------------------------|
| GET/POST | `/login/`             | `login_view`       | Authenticate user                            |
| GET/POST | `/register/`          | `register_view`    | Creates `User` + `UserProfile`             |
| GET    | `/logout/`              | `logout_view`      | Clear session                                |
| GET    | `/`                     | `home_view`        | Welcome screen (requires login)              |
| GET    | `/dashboard/`           | `dashboard`        | List all owned outlets                       |
| GET    | `/outlet/<id>/`         | `outlet_detail`    | Detailed view + history for an outlet        |
| POST   | `/outlet/<id>/toggle/`  | `toggle_outlet`    | Switch relay state (AJAX)                    |

### API App (`/api/`)
| Method | Route                    | View                  | Purpose                                   |
|:-------|:-------------------------|:----------------------|:------------------------------------------|
| POST   | `/api/sensor-data/`      | `receive_sensor_data` | ESP32 pushes new telemetry here           |
| GET    | `/api/outlet-status/<id>/`| `get_outlet_status`   | ESP32 polls to check if relay should be ON|

### Chatbot App (`/chat/`)
| Method | Route                  | View               | Purpose                                      |
|:-------|:-----------------------|:-------------------|:---------------------------------------------|
| GET    | `/chat/`               | `chat_page`        | Render the chat UI                           |
| POST   | `/chat/send/`          | `send_message`     | Process user prompt via Gemini API           |

---

## Authentication Flow

1. **Registration:** User submits `/register/`. Django creates a standard `User` object.
2. **Signals:** `outlets/models.py` uses `@receiver(post_save, sender=User)` to automatically generate an empty `UserProfile` for the new user. The view then populates `country` and `barangay`.
3. **Login:** User submits `/login/`. If valid, Django sets a session cookie.
4. **Protection:** Most views use `@login_required` to block anonymous access.

---

## API Endpoints (ESP32 Integration)

The `api` app provides REST endpoints strictly for the CCU firmware (ESP32) to communicate with the cloud.

### Push Sensor Data (`POST /api/sensor-data/`)
**Payload Format:**
```json
{
    "device_id": "FE",
    "voltage": 230.5,
    "current": 2.3,
    "power": 529.0,
    "energy": 1.25,
    "temperature": 35.2
}
```
**Action:** Saves a new `SensorData` row and broadcasts a WebSocket message to update the live UI.

### Poll Outlet Status (`GET /api/outlet-status/<device_id>/`)
**Response Format:**
```json
{
    "success": true,
    "device_id": "FE",
    "is_active": true,
    "name": "Desk Lamp"
}
```

---

## Chatbot (Gemini AI)

The `/chat/` interface uses `chatbot.gemini_client.GeminiClient` to communicate with Google's generative AI.

1. Takes API key from `GEMINI_API_KEY` in `.env`.
2. Loads a custom system instruction from `prompts/system_prompt.txt` to give the AI context about the Smart Outlet system.
3. User JS sends a POST request with the message to `/chat/send/`.
4. The server relays it to Gemini, formats the markdown response, and returns JSON.

---

## Environment Configuration (.env)

| Variable           | Default / Type | Purpose                                    |
|:-------------------|:---------------|:-------------------------------------------|
| `SECRET_KEY`       | String         | Django cryptographic signing key           |
| `DEBUG`            | `True`/`False` | Show detailed error pages?                 |
| `ALLOWED_HOSTS`    | `127.0.0.1,...`| Authorized domain names                    |
| `USE_LOCAL_DEV`    | `True`/`False` | **Toggles SQLite vs Supabase**             |
| `DB_NAME`...       | String         | PostgreSQL connection details              |
| `REDIS_HOST`/`PORT`| `localhost`    | WebSocket layer backplane                  |
| `GEMINI_API_KEY`   | String         | Key for the AI chatbot                     |

---

## Local Dev Mode (Offline Fallback)

If you need to develop offline or without access to the Supabase database and Redis server, you can toggle **Local Dev Mode**.

**Method:** 
1. Open `.env`.
2. Change `USE_LOCAL_DEV=False` to `USE_LOCAL_DEV=True`.
3. Restart the development server.

**Actions:**
Instead of attempting to connect to PostgreSQL and Channels Redis, `config/settings.py` will route to:
1. **Database:** A local file `db.sqlite3`.
2. **WebSockets:** `InMemoryChannelLayer` (uses RAM instead of a separate Redis instance).

> **Note:** Because `db.sqlite3` is a completely separate database file from Supabase, your user accounts and outlet data will **not** carry over when toggling this mode. You will need to run `python manage.py migrate` and create a new superuser locally when using this mode for the first time.

---

## File Structure

```text
Smart-Outlet-WebApp/
├── manage.py                          — Django admin utility script
├── config/                            — Project root settings
│   ├── settings.py                    — Core configuration, env routing
│   ├── urls.py                        — Root URL router
│   ├── asgi.py / wsgi.py              — Server entry points
│   └── routing.py                     — WebSocket URL router
│
├── outlets/                           — Core Web App
│   ├── models.py                      — SQLite/PostgreSQL schemas
│   ├── views.py                       — Auth, Home, Dashboard views
│   ├── urls.py                        — HTTP routes
│   └── consumers.py                   — WebSocket streaming logic
│
├── api/                               — ESP32 Integration
│   ├── views.py                       — JSON-in / JSON-out endpoints
│   └── urls.py                        — API routes
│
├── chatbot/                           — AI Assistant
│   ├── views.py                       — Chat UI, API bridge
│   ├── gemini_client.py               — Google SDK wrapper
│   └── prompts/system_prompt.txt      — AI instructions/persona
│
├── templates/                         — HTML Views
│   ├── base.html                      — Main layout skeleton
│   ├── index.html                     — Dashboard
│   ├── home.html                      — Welcome page
│   ├── auth/                          — Login/Register templates
│   └── chatbot/                       — Chat UI template
│
└── static/                            — Assets
    ├── css/style.css
    └── js/main.js, chatbot.js, websocket.js
```
