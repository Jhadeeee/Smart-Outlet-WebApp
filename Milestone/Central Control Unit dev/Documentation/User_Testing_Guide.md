# User Testing Guide — Smart Outlet Local Dashboard

**Firmware:** v3.0.0

## Overview

The Local Dashboard is a web UI served by the ESP32 for controlling PIC16F88 Smart Outlets via a browser. It is accessible in AP mode at `192.168.4.1` or via the device's LAN IP when connected to WiFi.

> [!NOTE]
> Device list is stored in RAM. All added devices will be lost on reboot.

---

## 1. Accessing the Dashboard

### Via Setup Mode (AP Mode)
1. Power on the ESP32 — it starts in **Setup Mode** by default.
2. Connect to the WiFi network: **CCU-Setup** (no password).
3. Open a browser and go to: `http://192.168.4.1`
4. On the setup page, tap **"⚡ Local Dashboard"** to enter the dashboard.

### Via Running Mode (STA Mode)
1. After configuring WiFi credentials on the setup page, the ESP32 connects to your home network.
2. Check the Serial Monitor (115200 baud) for the assigned IP address.
3. Navigate to `http://<assigned-ip>/dashboard` in your browser.

---

## 2. Adding a Device

1. Tap **"+ Add Device"** on the dashboard.
2. Enter a **name** (e.g., "Garage Outlet").
3. Enter the **PIC Device ID** in hex (e.g., `01`, `FE`).
4. Tap **Save**.

> [!IMPORTANT]
> The Device ID must match the ID programmed into the PIC16F88. The default PIC ID is `0x01`.

---

## 3. Controlling Relays

1. Tap a device row to **expand** it.
2. Use the **toggle switches** to control:
   - **Socket A** — Relay A (RA3 on PIC)
   - **Socket B** — Relay B (RA2 on PIC)
3. The toggle label shows **ON** (green) or **OFF** (red).

**Expected behavior:**
- Toggle ON → PIC acknowledges → toggle stays ON, label turns green.
- If no ACK → toggle may revert on next poll.

---

## 4. Current Readings (Auto-Polling)

When a device row is **expanded**, the dashboard automatically polls the PIC every **2 seconds**.

| Display | Meaning |
|:--------|:--------|
| `49 mA` | Idle current (no load) |
| `2205 mA` | 2.2A load detected |
| `-- mA` | No reading yet |

- **Socket A** and **Socket B** show independent current readings.
- The PIC reads from separate ACS712 sensors per socket.

> [!TIP]
> Polling pauses automatically when the context menu (⋮) is open or during config operations. It resumes when you're done.

---

## 5. Setting Overload Threshold

1. Expand a device row.
2. In the **Threshold** field, enter a value in mA (e.g., `3000` for 3A).
3. Tap **Set**.
4. The PIC will trip the relay if current exceeds this value.

---

## 6. Changing the Master ID

The Master ID identifies the ESP32 as the controller. All devices must recognize this ID to accept commands.

1. At the top of the dashboard, enter a new hex ID in the **Master ID** field (e.g., `0A`).
2. Tap **Set**.
3. The ESP32 sends `CMD_SET_ID_MASTER` to **each** listed device and waits for ACK.

**Expected result:**
- ✅ "Master ID set on all X device(s)" — all devices confirmed.
- ⚠️ "X/Y devices confirmed" — some devices didn't ACK.

> [!WARNING]
> The PIC must be in **Config Mode** to accept Master ID changes. Hold RB3 for **3 seconds** until you see `"Cfg!"` on the debug serial (LED blinks).

---

## 7. Changing a Device ID

1. Tap the **⋮** menu on a device row.
2. Select **"Change Device ID"**.
3. Enter the new hex ID (e.g., `AF`).
4. Tap **Send Command**.

**Expected result:**
- ✅ "Device ID changed successfully" — ACK received from new ID.
- ❌ "No ACK. Is PIC in config mode?" — PIC didn't respond.

> [!WARNING]
> The PIC must be in **Config Mode** before changing its Device ID. Hold RB3 for **3 seconds** until `"Cfg!"` appears.

> [!CAUTION]
> After changing the Device ID, the PIC will only respond to the **new ID**. To recover, press RB3 **3 times** quickly for a **Factory Reset** (resets Device ID, Master ID, and Threshold to defaults).

---

## 8. Renaming a Device

1. Tap the **⋮** menu on a device row.
2. Select **"Rename"**.
3. Enter the new name and tap **Save**.

This only changes the label on the dashboard. It does **not** affect the PIC.

---

## 9. Deleting a Device

1. Tap the **⋮** menu on a device row.
2. Select **"Delete"** (shown in red).
3. The device is removed from the list immediately.

> [!NOTE]
> This does not reset or power off the PIC. It only removes the device from the dashboard's RAM list.

---

## 10. WiFi Settings

1. Tap **"⚙ WiFi Settings"** at the bottom of the dashboard.
2. Enter your WiFi **SSID**, **Password**, and optionally a **Server URL**.
3. Tap **Save & Connect**.
4. The ESP32 will restart and connect to the specified network.

---

## Quick Reference — Test Checklist

| # | Test Case | Steps | Expected Result |
|:--|:----------|:------|:----------------|
| 1 | Add device | Tap +, enter name + ID `01`, Save | Device appears in list |
| 2 | Toggle Socket A ON | Expand device, toggle A ON | Label: ON (green), PIC relay clicks |
| 3 | Toggle Socket B ON | Toggle B ON | Label: ON (green), PIC relay clicks |
| 4 | Current reading | Wait 2-4 seconds | Both sockets show mA values |
| 5 | Set threshold | Enter 3000, tap Set | Toast: "Threshold: 3000 mA" |
| 6 | Rename device | ⋮ → Rename → new name → Save | Name updates in list |
| 7 | Change Device ID | Put PIC in config mode → ⋮ → Change ID → AF → Send | Toast: "Device ID changed successfully" |
| 8 | Change Master ID | Put PIC in config mode → Enter 0A → Set | Toast: "Master ID set on all X device(s)" |
| 9 | Delete device | ⋮ → Delete | Device removed from list |
| 10 | WiFi Settings | Tap ⚙, enter credentials, Save | ESP32 restarts and connects |

---

## Troubleshooting

| Problem | Cause | Solution |
|:--------|:------|:---------|
| Current shows `-- mA` | No poll response yet | Wait 2-4 seconds for auto-poll |
| Relay toggle doesn't stick | PIC didn't ACK | Check HC-12 wiring, distance, power |
| Device ID change fails | PIC not in config mode | Hold RB3 for **3 seconds** before changing |
| Master ID partial success | One PIC not responding | Retry, check PIC power/connection |
| Dashboard not loading | Not connected to CCU-Setup | Reconnect to AP, go to 192.168.4.1 |
| Devices gone after reboot | RAM-based storage | Re-add devices after each power cycle |
