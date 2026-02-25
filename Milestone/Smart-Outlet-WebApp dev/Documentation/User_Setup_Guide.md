# Smart-Outlet-WebApp — User Setup Guide

**This guide walks you through setting up the Django web application from scratch, connecting your ESP32, and using the online dashboard.**

---

## Prerequisites

| Requirement              | Version / Notes                           |
|:-------------------------|:------------------------------------------|
| Python                   | 3.10+                                     |
| PostgreSQL               | 14+ (with a database created for the app) |
| Git                      | For cloning the repository                |
| ESP32 with CCU Firmware  | v4.0.0+ (already flashed and WiFi configured) |
| PIC Smart Outlet         | With Device ID set (e.g., `0xFE`)         |

---

## Step 1: Clone the Repository

```bash
git clone https://github.com/Jhadeeee/Smart-Outlet-WebApp.git
cd Smart-Outlet-WebApp
```

---

## Step 2: Create a Virtual Environment

```bash
# Windows
python -m venv .venv
.venv\Scripts\activate

# Linux / macOS
python3 -m venv .venv
source .venv/bin/activate
```

---

## Step 3: Install Dependencies

```bash
pip install -r requirements.txt
```

---

## Step 4: Configure Environment Variables

Copy the example `.env` file and edit it:

```bash
copy .env.example .env
```

Edit `.env` with your PostgreSQL credentials:

```env
SECRET_KEY=your-django-secret-key
DEBUG=True

DB_NAME=smart_outlet_db
DB_USER=postgres
DB_PASSWORD=your_password
DB_HOST=localhost
DB_PORT=5432
```

---

## Step 5: Set Up the Database

```bash
python manage.py makemigrations
python manage.py migrate
python manage.py createsuperuser
```

Follow the prompts to create your admin account (username, email, password).

---

## Step 6: Start the Development Server

```bash
python manage.py runserver 0.0.0.0:8000
```

> **Important:** Use `0.0.0.0:8000` (not `127.0.0.1:8000`) so the ESP32 can reach the server from the local network.

Open your browser and go to `http://localhost:8000/`

---

## Step 7: Register Your CCU (ESP32)

1. Log in with your superuser credentials
2. On the home page, click **"+ Add CCU"**
3. Enter the CCU details:
   - **CCU Name:** e.g., `My CCU`
   - **CCU ID:** Must match the `CCU_SENDER_ID` in the firmware (default: `01`)
   - **Location:** Optional
4. Click **"Register CCU"**

---

## Step 8: Register Your Smart Outlet

1. Click **"+ Add Outlet"**
2. Enter the outlet details:
   - **Outlet Name:** e.g., `Living Room Outlet`
   - **Device ID (Hex):** Must match the PIC's Device ID (e.g., `FE`)
   - **Location:** Optional
3. Click **"Add Outlet"**

---

## Step 9: Configure the ESP32

The ESP32 needs to know the Django server address.

1. If the ESP32 has never been configured, it starts in **AP mode** (hotspot name: `CCU-Setup`)
2. Connect your phone/laptop to the `CCU-Setup` WiFi
3. A captive portal will open automatically. Fill in:
   - **SSID:** Your WiFi network name
   - **Password:** Your WiFi password
   - **Server URL:** `http://<your_PC_IP>:8000` (e.g., `http://10.31.253.107:8000`)
4. Click **Save & Connect**

> **Finding your PC's IP address:**
> ```bash
> # Windows
> ipconfig
> 
> # Linux / macOS
> ifconfig
> ```
> Look for the IPv4 address on your WiFi adapter (e.g., `10.31.253.107`).

---

## Step 10: Verify the Connection

Once the ESP32 connects to WiFi and the server:

1. Check the Django terminal — you should see requests like:
   ```
   HTTP GET /api/devices/ 200
   HTTP POST /api/data/ 200
   HTTP POST /api/breaker-data/ 200
   HTTP GET /api/commands/FE/ 200
   ```

2. The ESP32 Serial Monitor should show:
   ```
   ✓ Server is reachable: http://10.31.253.107:8000
     Syncing device list from server...
     ✓ Synced 1 device(s) from server.
   ✓ HC-12 RF + Serial CLI + Dashboard ready.
   ```

3. The web dashboard should now show **real-time current readings** and allow you to **toggle relays** on/off.

---

## Using the Dashboard

### Toggle Relays
Click the toggle switches next to **Socket A** or **Socket B** to turn relays ON/OFF. The command is queued in the database and the ESP32 picks it up within ~2 seconds.

### Set Overload Threshold
1. Enter a threshold value in the **Threshold** input (e.g., `3000` for 3A)
2. Click **Set**
3. The threshold is sent to both the Django database and the PIC via the ESP32

### Monitor Current
The Socket A/B current values update in real-time via WebSocket. No page refresh needed.

### Emergency Cut-Off
Click **⛔ Cut All Power** to immediately turn OFF all relays on all registered outlets.

---

## Troubleshooting

### ESP32 can't reach the server

| Symptom | Solution |
|:--------|:---------|
| `✗ Server not reachable` in Serial Monitor | Check that Django is running with `0.0.0.0:8000`, not `127.0.0.1:8000` |
| No HTTP requests in Django terminal | Verify the Server URL in the ESP32 matches your PC's IP |
| ESP32 IP is `0.0.0.0` | WiFi connection failed — check SSID/password |

### Relays show ON after ESP32 reboot

This was fixed in the `first_stable_UI` tag. Make sure you're running the latest firmware. The fix ensures that the default unknown relay state (`-1`) is reported as OFF, not ON.

### No overload alerts in database

Check two things:
1. The firmware must report `is_overload: true` when current is `65535` (0xFFFF)
2. Django's `Outlet.threshold` must be set correctly — this is separate from the PIC's threshold

### WebSocket not updating

- Check the browser console for WebSocket errors
- Ensure `django-channels` is installed and `CHANNEL_LAYERS` is configured in `settings.py`

### Factory Reset the ESP32

Hold the **BOOT** button (GPIO 0) for 3+ seconds during startup. This clears the WiFi credentials and server URL, forcing the ESP32 back into AP setup mode.

---

## Adding More Outlets

1. **Physically:** Set the PIC's Device ID using the ESP32's Serial CLI or local dashboard
   - Example: `d FD` sets the PIC to device `0xFD`
2. **On Django:** Click **"+ Add Outlet"** and enter the same Device ID (`FD`)
3. The ESP32 will detect the new device within 2 seconds via the `/api/devices/` sync

> **Note:** No ESP32 reboot is required when adding new outlets.
