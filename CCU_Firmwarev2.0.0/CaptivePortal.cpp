/*
 * CaptivePortal.cpp
 * -------------------
 * Implementation of the captive portal web server.
 * Serves a styled HTML form for WiFi credential input.
 */

#include "CaptivePortal.h"

// ─── DNS Server Config ──────────────────────────────────────
#define DNS_PORT 53

CaptivePortal::CaptivePortal(ConfigStorage& configStorage)
    : _server(WEB_SERVER_PORT),
      _configStorage(configStorage),
      _submitted(false) {}

void CaptivePortal::begin() {
    // Start DNS server — redirect ALL domains to our IP (captive portal)
    _dnsServer.start(DNS_PORT, "*", AP_IP);

    // Register HTTP routes
    _server.on("/",      HTTP_GET,  [this]() { _handleRoot(); });
    _server.on("/save",  HTTP_POST, [this]() { _handleSubmit(); });
    _server.onNotFound(              [this]() { _handleNotFound(); });

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

// ─── Route: Root Page ───────────────────────────────────────
void CaptivePortal::_handleRoot() {
    _server.send(200, "text/html", _buildSetupPage());
}

// ─── Route: Form Submission ─────────────────────────────────
void CaptivePortal::_handleSubmit() {
    String ssid      = _server.arg("ssid");
    String password  = _server.arg("password");
    String serverUrl = _server.arg("serverUrl");

    // Basic validation
    if (ssid.length() == 0 || serverUrl.length() == 0) {
        _server.send(400, "text/html",
            "<html><body style='background:#1a1a2e;color:#e94560;font-family:sans-serif;text-align:center;padding:40px;'>"
            "<h2>&#9888; Error</h2><p>SSID and Server URL are required.</p>"
            "<a href='/' style='color:#0f3460;'>Go Back</a></body></html>");
        return;
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

        .footer {
            text-align: center;
            margin-top: 20px;
            color: #555570;
            font-size: 11px;
        }
    </style>
</head>
<body>
    <div class="card">
        <div class="logo">&#9889;</div>
        <h1>CCU WiFi Setup</h1>
        <p class="subtitle">Connect your Central Control Unit to the network</p>

        <form action="/save" method="POST">
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
                <input type="url" name="serverUrl" placeholder="http://your-server.com" required>
            </div>

            <button type="submit" class="btn">Save &amp; Connect</button>
        </form>

        <div class="footer">CCU Firmware &bull; ESP32</div>
    </div>
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
