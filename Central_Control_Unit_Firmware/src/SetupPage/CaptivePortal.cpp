/*
 * CaptivePortal.cpp
 * -------------------
 * Implementation of the captive portal web server.
 * Serves a styled HTML form for WiFi credential input.
 */

#include "CaptivePortal.h"
#include <WiFi.h>

// ─── DNS Server Config ──────────────────────────────────────
#define DNS_PORT 53

CaptivePortal::CaptivePortal(ConfigStorage& configStorage)
    : _server(WEB_SERVER_PORT),
      _configStorage(configStorage),
      _submitted(false),
      _dashboardRequested(false) {}

void CaptivePortal::begin() {
    // Start DNS server — redirect ALL domains to our IP (captive portal)
    _dnsServer.start(DNS_PORT, "*", AP_IP);

    // Register HTTP routes
    _server.on("/",          HTTP_GET,  [this]() { _handleRoot(); });
    _server.on("/save",      HTTP_POST, [this]() { _handleSubmit(); });
    _server.on("/dashboard", HTTP_GET,  [this]() { _handleDashboardRequest(); });
    _server.on("/scan",      HTTP_GET,  [this]() { _handleScanWifi(); });
    _server.onNotFound(                  [this]() { _handleNotFound(); });

    _server.begin();
    Serial.println("[CaptivePortal] Web server started on port " + String(WEB_SERVER_PORT));
}

void CaptivePortal::stop() {
    _server.stop();
    _dnsServer.stop();
    Serial.println("[CaptivePortal] Servers stopped.");
}

void CaptivePortal::handleClient() {
    _dnsServer.processNextRequest();
    _server.handleClient();
}

bool CaptivePortal::isSubmitted() const {
    return _submitted;
}

bool CaptivePortal::isDashboardRequested() const {
    return _dashboardRequested;
}

// ─── Route: Dashboard Request ───────────────────────────────
void CaptivePortal::_handleDashboardRequest() {
    _server.send(200, "text/html",
        "<html><body style='background:#0f0c29;color:#4ecca3;font-family:sans-serif;"
        "text-align:center;padding:60px;'>"
        "<h2>&#9889; Starting Local Dashboard...</h2>"
        "<p style='color:#8888aa;'>Please wait...</p></body></html>");
    _dashboardRequested = true;
    Serial.println("[CaptivePortal] Local Dashboard requested.");
}

// ─── Route: Root Page ───────────────────────────────────────
void CaptivePortal::_handleRoot() {
    _server.send(200, "text/html", _buildSetupPage());
}

// ─── Route: Form Submission ─────────────────────────────────
void CaptivePortal::_handleSubmit() {
    String ssid      = _server.arg("ssid");
    String password  = _server.arg("password");
    String serverUrl = _server.arg("serverUrl");

    // Basic validation — only SSID is required (Server URL is optional)
    if (ssid.length() == 0) {
        _server.send(400, "text/html",
            "<html><body style='background:#1a1a2e;color:#e94560;font-family:sans-serif;text-align:center;padding:40px;'>"
            "<h2>&#9888; Error</h2><p>WiFi SSID is required.</p>"
            "<a href='/' style='color:#0f3460;'>Go Back</a></body></html>");
        return;
    }

    // Normalize Server URL
    if (serverUrl.length() > 0) {
        if (!serverUrl.startsWith("http://") && !serverUrl.startsWith("https://")) {
            serverUrl = "http://" + serverUrl;
        }
        if (serverUrl.endsWith("/")) {
            serverUrl = serverUrl.substring(0, serverUrl.length() - 1);
        }
    }

    // Save credentials
    _configStorage.save(ssid, password, serverUrl);
    _submitted = true;

    // Send success page
    _server.send(200, "text/html", _buildSuccessPage(serverUrl));

    Serial.println("[CaptivePortal] Credentials submitted. Restarting in 3 seconds...");

    // Delay to let the page render, then restart
    delay(3000);
    ESP.restart();
}

// ─── Route: WiFi Scan (Async) ───────────────────────────────
void CaptivePortal::_handleScanWifi() {
    int result = WiFi.scanComplete();

    if (result == -2) {
        // No scan running → start one (async / non-blocking)
        WiFi.scanNetworks(true);
        Serial.println("[CaptivePortal] WiFi scan started (async)...");
        _server.send(200, "application/json", "{\"status\":\"scanning\"}");
    }
    else if (result == -1) {
        // Scan still in progress
        _server.send(200, "application/json", "{\"status\":\"scanning\"}");
    }
    else {
        // Scan complete — build and return results
        Serial.println("[CaptivePortal] Scan complete: " + String(result) + " networks");
        String json = _scanNetworksJSON();
        WiFi.scanDelete();  // Free scan results memory
        _server.send(200, "application/json", json);
    }
}

String CaptivePortal::_scanNetworksJSON() {
    int n = WiFi.scanComplete();
    String json = "[";

    String seen[20];
    int uniqueCount = 0;

    for (int i = 0; i < n && uniqueCount < 20; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;  // Skip hidden networks

        // De-duplicate (same SSID from multiple APs)
        bool dup = false;
        for (int j = 0; j < uniqueCount; j++) {
            if (seen[j] == ssid) { dup = true; break; }
        }
        if (dup) continue;
        seen[uniqueCount++] = ssid;

        if (uniqueCount > 1) json += ",";
        json += "{\"ssid\":\"" + ssid + "\","
                "\"rssi\":" + String(WiFi.RSSI(i)) + ","
                "\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN
                               ? "true" : "false") + "}";
    }
    json += "]";
    return json;
}

// ─── Route: 404 → Redirect to Root (Captive Portal) ────────
void CaptivePortal::_handleNotFound() {
    _server.sendHeader("Location", "http://" + AP_IP.toString(), true);
    _server.send(302, "text/plain", "Redirecting to setup...");
}

// ─── HTML: Setup Page with Styled Form ──────────────────────
String CaptivePortal::_buildSetupPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CCU WiFi Setup</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #0f0c29, #302b63, #24243e);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }

        .card {
            background: rgba(255, 255, 255, 0.05);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 20px;
            padding: 40px 32px;
            width: 100%;
            max-width: 400px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
        }

        .logo {
            text-align: center;
            margin-bottom: 8px;
            font-size: 40px;
        }

        h1 {
            color: #e0e0ff;
            text-align: center;
            font-size: 22px;
            font-weight: 600;
            margin-bottom: 6px;
        }

        .subtitle {
            color: #8888aa;
            text-align: center;
            font-size: 13px;
            margin-bottom: 30px;
        }

        .form-group {
            margin-bottom: 20px;
        }

        label {
            display: block;
            color: #b0b0cc;
            font-size: 13px;
            font-weight: 500;
            margin-bottom: 6px;
            letter-spacing: 0.5px;
        }

        input[type="text"],
        input[type="password"],
        input[type="url"] {
            width: 100%;
            padding: 12px 16px;
            background: rgba(255, 255, 255, 0.08);
            border: 1px solid rgba(255, 255, 255, 0.15);
            border-radius: 10px;
            color: #ffffff;
            font-size: 15px;
            outline: none;
            transition: border-color 0.3s, box-shadow 0.3s;
        }

        input:focus {
            border-color: #7c5cbf;
            box-shadow: 0 0 0 3px rgba(124, 92, 191, 0.25);
        }

        input::placeholder {
            color: #666680;
        }

        .btn {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, #7c5cbf, #e94560);
            border: none;
            border-radius: 10px;
            color: #fff;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            letter-spacing: 0.5px;
            transition: transform 0.2s, box-shadow 0.3s;
            margin-top: 10px;
        }

        .btn:hover {
            transform: translateY(-1px);
            box-shadow: 0 6px 20px rgba(233, 69, 96, 0.4);
        }

        .btn:active {
            transform: translateY(0);
        }

        .divider {
            display: flex;
            align-items: center;
            margin: 20px 0;
            color: #555570;
            font-size: 12px;
        }

        .divider::before,
        .divider::after {
            content: '';
            flex: 1;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }

        .divider span {
            padding: 0 12px;
        }

        .btn-outline {
            display: block;
            text-align: center;
            text-decoration: none;
            padding: 14px;
            background: transparent;
            border: 1px solid rgba(124, 92, 191, 0.5);
            border-radius: 10px;
            color: #b0b0cc;
            font-size: 15px;
            font-weight: 500;
            transition: all 0.3s;
        }

        .btn-outline:hover {
            background: rgba(124, 92, 191, 0.15);
            border-color: #7c5cbf;
            color: #e0e0ff;
        }

        .footer {
            text-align: center;
            margin-top: 20px;
            color: #555570;
            font-size: 11px;
        }

        .btn-scan {
            width: 100%; padding: 10px;
            background: rgba(124,92,191,0.2);
            border: 1px dashed rgba(124,92,191,0.5);
            border-radius: 10px; color: #b0b0cc;
            font-size: 14px; cursor: pointer;
            margin-bottom: 12px; transition: all 0.3s;
        }
        .btn-scan:hover { background: rgba(124,92,191,0.35); color: #e0e0ff; }
        .spinner { text-align:center; color:#7c5cbf; padding:12px; font-size:13px; }
        .wifi-list { max-height:200px; overflow-y:auto; margin-bottom:12px; }
        .wifi-item {
            display:flex; justify-content:space-between; align-items:center;
            padding:10px 14px; background:rgba(255,255,255,0.06);
            border:1px solid rgba(255,255,255,0.1);
            border-radius:8px; margin-bottom:6px;
            cursor:pointer; transition:background 0.2s;
        }
        .wifi-item:hover { background:rgba(124,92,191,0.2); }
        .wifi-name { color:#e0e0ff; font-size:14px; }
        .wifi-meta { color:#8888aa; font-size:12px; }
        .wifi-list { overscroll-behavior:contain; -webkit-overflow-scrolling:touch; }
        .wifi-list::-webkit-scrollbar { width:6px; }
        .wifi-list::-webkit-scrollbar-track { background:rgba(255,255,255,0.05); border-radius:3px; }
        .wifi-list::-webkit-scrollbar-thumb { background:rgba(124,92,191,0.4); border-radius:3px; }
        .wifi-list::-webkit-scrollbar-thumb:hover { background:rgba(124,92,191,0.6); }
    </style>
</head>
<body>
    <div class="card">
        <div class="logo">&#9889;</div>
        <h1>CCU WiFi Setup</h1>
        <p class="subtitle">Connect your Central Control Unit to the network</p>

        <form action="/save" method="POST">
            <button type="button" class="btn-scan" onclick="scanWifi()">&#128225; Scan WiFi</button>
            <div id="scanSpinner" class="spinner" style="display:none;">Scanning nearby networks...</div>
            <div id="scanResults" style="display:none;"></div>

            <div class="form-group">
                <label>WiFi Network Name (SSID)</label>
                <input type="text" name="ssid" placeholder="Enter your WiFi SSID" required>
            </div>

            <div class="form-group">
                <label>WiFi Password</label>
                <input type="password" name="password" placeholder="Enter your WiFi password">
            </div>

            <div class="form-group">
                <label>Server URL</label>
                <input type="text" name="serverUrl" placeholder="e.g. 192.168.1.6:8000 (optional)">
            </div>

            <button type="submit" class="btn">Save &amp; Connect</button>
        </form>

        <div class="divider"><span>or</span></div>
        <a href="/dashboard" class="btn btn-outline">&#9881; Local Dashboard</a>

        <div class="footer">CCU Firmware &bull; ESP32</div>
    </div>

    <script>
    function scanWifi() {
        document.getElementById('scanSpinner').style.display = 'block';
        document.getElementById('scanResults').style.display = 'none';
        pollScan();
    }
    function pollScan() {
        fetch('/scan').then(r => r.json()).then(data => {
            if (data.status === 'scanning') { setTimeout(pollScan, 500); return; }
            document.getElementById('scanSpinner').style.display = 'none';
            data.sort((a,b) => b.rssi - a.rssi);
            let html = '<div class="wifi-list">';
            if (data.length === 0) {
                html += '<div style="text-align:center;color:#8888aa;padding:12px;">No networks found</div>';
            }
            data.forEach(n => {
                let bars = n.rssi > -50 ? '\u25B0\u25B0\u25B0\u25B0' :
                           n.rssi > -65 ? '\u25B0\u25B0\u25B0 ' :
                           n.rssi > -80 ? '\u25B0\u25B0  ' : '\u25B0   ';
                let lock = n.open ? '\uD83D\uDD13' : '\uD83D\uDD12';
                html += '<div class="wifi-item" onclick="selectWifi(\'' +
                    n.ssid.replace(/'/g,"\\'") + '\',' + n.open + ')">' +
                    '<div><div class="wifi-name">' + bars + ' ' + n.ssid + '</div>' +
                    '<div class="wifi-meta">' + n.rssi + ' dBm</div></div>' +
                    '<span>' + lock + '</span></div>';
            });
            html += '</div>';
            let el = document.getElementById('scanResults');
            el.innerHTML = html;
            el.style.display = 'block';
        }).catch(() => {
            document.getElementById('scanSpinner').style.display = 'none';
            alert('Scan failed. Try again.');
        });
    }
    function selectWifi(ssid, isOpen) {
        document.querySelector('input[name="ssid"]').value = ssid;
        let pw = document.querySelector('input[name="password"]');
        if (isOpen) {
            pw.value = ''; pw.disabled = true;
            pw.placeholder = 'No password required';
        } else {
            pw.disabled = false;
            pw.placeholder = 'Enter your WiFi password';
            pw.focus();
        }
        document.getElementById('scanResults').style.display = 'none';
    }
    </script>
</body>
</html>
)rawliteral";

    return html;
}

// ─── HTML: Success Page ─────────────────────────────────────
String CaptivePortal::_buildSuccessPage(const String& serverUrl) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Setup Complete</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #0f0c29, #302b63, #24243e);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .card {
            background: rgba(255, 255, 255, 0.05);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 20px;
            padding: 40px 32px;
            width: 100%;
            max-width: 400px;
            text-align: center;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
        }
        .icon { font-size: 50px; margin-bottom: 16px; }
        h1 { color: #4ecca3; font-size: 22px; margin-bottom: 12px; }
        p { color: #b0b0cc; font-size: 14px; line-height: 1.6; }
        .server-url {
            display: inline-block;
            margin-top: 16px;
            padding: 10px 20px;
            background: rgba(78, 204, 163, 0.15);
            border: 1px solid rgba(78, 204, 163, 0.3);
            border-radius: 8px;
            color: #4ecca3;
            font-size: 13px;
            word-break: break-all;
        }
        .note {
            margin-top: 20px;
            color: #666680;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <div class="card">
        <div class="icon">&#10004;</div>
        <h1>Setup Complete!</h1>
        <p>Your CCU is restarting and connecting to your WiFi network.</p>
        <p>Once connected, access your server at:</p>
        <div class="server-url">)rawliteral";

    html += serverUrl;
    html += R"rawliteral(</div>
        <p class="note">Disconnect from "CCU-Setup" and reconnect to your home WiFi to access the server.</p>
    </div>
</body>
</html>
)rawliteral";

    return html;
}
