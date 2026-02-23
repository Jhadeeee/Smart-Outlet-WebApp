/*
 * Dashboard.cpp
 * --------------
 * Local web dashboard with device list UI, toggle switches,
 * auto-polling current values, and global Master ID.
 */

#include "Dashboard.h"

Dashboard::Dashboard(OutletManager& manager, ConfigStorage& config)
    : _server(WEB_SERVER_PORT),
      _manager(manager),
      _config(config) {}

void Dashboard::begin() {
    // Page routes
    _server.on("/",          HTTP_GET,  [this]() { _handleDashboard(); });
    _server.on("/dashboard", HTTP_GET,  [this]() { _handleDashboard(); });
    _server.on("/settings",  HTTP_GET,  [this]() { _handleSettings(); });
    _server.on("/settings/save", HTTP_POST, [this]() { _handleSaveSettings(); });

    // Device CRUD API
    _server.on("/api/devices",          HTTP_GET,  [this]() { _handleApiDeviceList(); });
    _server.on("/api/devices/add",      HTTP_POST, [this]() { _handleApiAddDevice(); });
    _server.on("/api/devices/rename",   HTTP_POST, [this]() { _handleApiRenameDevice(); });
    _server.on("/api/devices/delete",   HTTP_POST, [this]() { _handleApiDeleteDevice(); });
    _server.on("/api/devices/changeId", HTTP_POST, [this]() { _handleApiChangeDeviceId(); });

    // Control API
    _server.on("/api/relay",     HTTP_POST, [this]() { _handleApiRelay(); });
    _server.on("/api/threshold", HTTP_POST, [this]() { _handleApiSetThreshold(); });
    _server.on("/api/master",    HTTP_POST, [this]() { _handleApiSetMasterID(); });
    _server.on("/api/status",    HTTP_GET,  [this]() { _handleApiStatus(); });
    _server.on("/api/sensors",   HTTP_POST, [this]() { _handleApiReadSensors(); });

    _server.begin();
    Serial.println("[Dashboard] Web server started on port " + String(WEB_SERVER_PORT));
}

void Dashboard::stop() {
    _server.stop();
    Serial.println("[Dashboard] Web server stopped.");
}

void Dashboard::handleClient() {
    _server.handleClient();
}

// ════════════════════════════════════════════════════════════
//  PAGE ROUTES
// ════════════════════════════════════════════════════════════

void Dashboard::_handleDashboard() {
    _server.send(200, "text/html", _buildDashboardPage());
}

void Dashboard::_handleSettings() {
    _server.send(200, "text/html", _buildSettingsPage());
}

void Dashboard::_handleSaveSettings() {
    String ssid      = _server.arg("ssid");
    String password  = _server.arg("password");
    String serverUrl = _server.arg("serverUrl");

    if (ssid.length() == 0) {
        _server.send(400, "application/json", "{\"error\":\"SSID is required\"}");
        return;
    }

    _config.save(ssid, password, serverUrl);
    _server.send(200, "text/html",
        "<html><body style='background:#0f0c29;color:#4ecca3;font-family:sans-serif;"
        "text-align:center;padding:60px;'>"
        "<h2>&#10004; Settings Saved!</h2>"
        "<p style='color:#8888aa;'>Restarting and connecting to WiFi...</p>"
        "</body></html>");

    Serial.println("[Dashboard] WiFi settings saved. Restarting...");
    delay(2000);
    ESP.restart();
}

// ════════════════════════════════════════════════════════════
//  DEVICE CRUD API
// ════════════════════════════════════════════════════════════

void Dashboard::_handleApiDeviceList() {
    String json = "[";
    for (uint8_t i = 0; i < _manager.getDeviceCount(); i++) {
        if (i > 0) json += ",";
        OutletDevice& d = _manager.getDevice(i);
        json += "{\"index\":";
        json += String(i);
        json += ",\"name\":\"";
        json += String(d.getName());
        json += "\",\"id\":\"0x";
        if (d.getDeviceId() < 0x10) json += "0";
        json += String(d.getDeviceId(), HEX);
        json += "\"}";
    }
    json += "]";
    _server.send(200, "application/json", json);
}

void Dashboard::_handleApiAddDevice() {
    String name = _server.arg("name");
    String idStr = _server.arg("id");

    if (name.length() == 0 || idStr.length() == 0) {
        _server.send(400, "application/json", "{\"error\":\"name and id required\"}");
        return;
    }
    if (_manager.getDeviceCount() >= MAX_OUTLETS) {
        _server.send(400, "application/json", "{\"error\":\"Max devices reached\"}");
        return;
    }

    uint8_t id = (uint8_t)strtol(idStr.c_str(), NULL, 16);
    _manager.selectDevice(id);

    // Set the name on the device
    int idx = -1;
    for (uint8_t i = 0; i < _manager.getDeviceCount(); i++) {
        if (_manager.getDevice(i).getDeviceId() == id) {
            _manager.getDevice(i).setName(name.c_str());
            idx = i;
            break;
        }
    }

    Serial.print("[Dashboard] Added device: ");
    Serial.print(name);
    Serial.print(" (0x");
    if (id < 0x10) Serial.print("0");
    Serial.print(id, HEX);
    Serial.println(")");

    _server.send(200, "application/json", "{\"ok\":true,\"index\":" + String(idx) + "}");
}

void Dashboard::_handleApiRenameDevice() {
    uint8_t index = (uint8_t)_server.arg("index").toInt();
    String name = _server.arg("name");

    if (index >= _manager.getDeviceCount() || name.length() == 0) {
        _server.send(400, "application/json", "{\"error\":\"Invalid index or name\"}");
        return;
    }

    _manager.getDevice(index).setName(name.c_str());
    Serial.println("[Dashboard] Renamed device " + String(index) + " to: " + name);
    _server.send(200, "application/json", "{\"ok\":true}");
}

void Dashboard::_handleApiDeleteDevice() {
    uint8_t index = (uint8_t)_server.arg("index").toInt();

    if (index >= _manager.getDeviceCount()) {
        _server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
        return;
    }

    Serial.print("[Dashboard] Deleted device: ");
    Serial.println(_manager.getDevice(index).getName());

    _manager.removeDevice(index);
    _server.send(200, "application/json", "{\"ok\":true}");
}

void Dashboard::_handleApiChangeDeviceId() {
    uint8_t index = (uint8_t)_server.arg("index").toInt();
    String newIdStr = _server.arg("newId");

    if (index >= _manager.getDeviceCount() || newIdStr.length() == 0) {
        _server.send(400, "application/json", "{\"error\":\"Invalid index or newId\"}");
        return;
    }

    uint8_t oldId = _manager.getDevice(index).getDeviceId();
    uint8_t newId = (uint8_t)strtol(newIdStr.c_str(), NULL, 16);
    String devName = String(_manager.getDevice(index).getName());

    // Select this device and send CMD_SET_DEVICE_ID
    _manager.selectDevice(oldId);
    _manager.setDeviceID(newId);

    Serial.print("[Dashboard] Change Device ID 0x");
    if (oldId < 0x10) Serial.print("0");
    Serial.print(oldId, HEX);
    Serial.print(" -> 0x");
    if (newId < 0x10) Serial.print("0");
    Serial.println(newId, HEX);

    // Block up to 3 seconds waiting for ACK from the NEW ID
    bool ackReceived = false;
    unsigned long start = millis();
    while (millis() - start < 3000) {
        _manager.update();  // Process incoming packets

        // Check if ACK came from the new ID
        if (_manager.getLastAckSender() == newId) {
            ackReceived = true;
            break;
        }
        delay(50);
    }

    // Update device record regardless (PIC already changed its ID)
    _manager.getDevice(index).init(newId);
    _manager.getDevice(index).setName(devName.c_str());

    if (ackReceived) {
        Serial.println("[Dashboard] Device ID change confirmed.");
        _server.send(200, "application/json", "{\"success\":true}");
    } else {
        Serial.println("[Dashboard] Device ID change — no ACK (timeout).");
        _server.send(200, "application/json", "{\"success\":false,\"reason\":\"No ACK. Is PIC in config mode?\"}");
    }
}

// ════════════════════════════════════════════════════════════
//  CONTROL API
// ════════════════════════════════════════════════════════════

void Dashboard::_handleApiRelay() {
    uint8_t index = (uint8_t)_server.arg("index").toInt();
    uint8_t socket = (uint8_t)_server.arg("socket").toInt();
    String state = _server.arg("state");

    if (index >= _manager.getDeviceCount()) {
        _server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
        return;
    }
    if (socket != SOCKET_A && socket != SOCKET_B) {
        _server.send(400, "application/json", "{\"error\":\"socket must be 1 or 2\"}");
        return;
    }

    // Select the target device
    _manager.selectDevice(_manager.getDevice(index).getDeviceId());

    if (state == "on") {
        _manager.relayOn(socket);
    } else {
        _manager.relayOff(socket);
    }

    _server.send(200, "application/json", "{\"ok\":true}");
}

void Dashboard::_handleApiSetThreshold() {
    uint8_t index = (uint8_t)_server.arg("index").toInt();
    unsigned int mA = (unsigned int)_server.arg("value").toInt();

    if (index >= _manager.getDeviceCount()) {
        _server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
        return;
    }
    if (mA == 0) {
        _server.send(400, "application/json", "{\"error\":\"value must be > 0\"}");
        return;
    }

    _manager.selectDevice(_manager.getDevice(index).getDeviceId());
    _manager.setThreshold(mA);
    _server.send(200, "application/json", "{\"ok\":true}");
}

void Dashboard::_handleApiSetMasterID() {
    String valStr = _server.arg("value");
    if (valStr.length() == 0) {
        _server.send(400, "application/json", "{\"error\":\"value required\"}");
        return;
    }

    uint8_t newId = (uint8_t)strtol(valStr.c_str(), NULL, 16);
    uint8_t total = _manager.getDeviceCount();
    uint8_t success = 0;

    // Send CMD_SET_ID_MASTER to each device, wait for ACK
    for (uint8_t i = 0; i < total; i++) {
        _manager.selectDevice(_manager.getDevice(i).getDeviceId());
        _manager.setMasterID(newId);

        // Wait up to 3 seconds for ACK
        bool acked = false;
        unsigned long start = millis();
        while (millis() - start < 3000) {
            _manager.update();
            if (_manager.getDevice(i).getMasterID() == (int)newId) {
                acked = true;
                break;
            }
            delay(50);
        }
        if (acked) success++;

        Serial.print("[Dashboard] Master ID -> device ");
        Serial.print(i);
        Serial.println(acked ? ": ACK" : ": TIMEOUT");
    }

    // Update ESP32's own sender ID to match
    _manager.setSenderID(newId);

    String json = "{\"success\":" + String(success) + ",\"total\":" + String(total) + "}";
    _server.send(200, "application/json", json);
}

void Dashboard::_handleApiStatus() {
    uint8_t index = (uint8_t)_server.arg("index").toInt();

    if (index >= _manager.getDeviceCount()) {
        _server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
        return;
    }

    OutletDevice& dev = _manager.getDevice(index);

    String json = "{";
    json += "\"id\":\"0x";
    if (dev.getDeviceId() < 0x10) json += "0";
    json += String(dev.getDeviceId(), HEX) + "\",";
    json += "\"name\":\"" + String(dev.getName()) + "\",";
    json += "\"relayA\":" + String(dev.getRelayA()) + ",";
    json += "\"relayB\":" + String(dev.getRelayB()) + ",";
    json += "\"currentA\":" + String(dev.getCurrentA()) + ",";
    json += "\"currentB\":" + String(dev.getCurrentB()) + ",";
    json += "\"threshold\":" + String(dev.getThreshold()) + ",";
    json += "\"masterID\":" + String(dev.getMasterID());
    json += "}";

    _server.send(200, "application/json", json);
}

void Dashboard::_handleApiReadSensors() {
    uint8_t index = (uint8_t)_server.arg("index").toInt();

    if (index >= _manager.getDeviceCount()) {
        _server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
        return;
    }

    _manager.selectDevice(_manager.getDevice(index).getDeviceId());
    _manager.readSensors();
    _server.send(200, "application/json", "{\"ok\":true}");
}

// ════════════════════════════════════════════════════════════
//  HTML: DASHBOARD PAGE
// ════════════════════════════════════════════════════════════

String Dashboard::_buildDashboardPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Outlet Dashboard</title>
    <style>
        *{margin:0;padding:0;box-sizing:border-box}
        body{font-family:'Segoe UI',Tahoma,sans-serif;background:linear-gradient(135deg,#0f0c29,#302b63,#24243e);min-height:100vh;color:#e0e0ff;padding:16px}
        .ctr{max-width:480px;margin:0 auto}
        .hdr{text-align:center;margin-bottom:20px}
        .hdr h1{font-size:20px;font-weight:600}
        .hdr .sub{color:#8888aa;font-size:12px}
        .card{background:rgba(255,255,255,0.05);backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,0.1);border-radius:14px;padding:16px;margin-bottom:12px}
        .card-t{color:#7c5cbf;font-size:11px;font-weight:600;text-transform:uppercase;letter-spacing:1px;margin-bottom:10px}

        /* Master ID */
        .master-row{display:flex;gap:8px;align-items:center}
        .master-show{color:#4ecca3;font-size:15px;font-weight:600;flex:1}
        .inp-sm{padding:8px 10px;background:rgba(255,255,255,0.08);border:1px solid rgba(255,255,255,0.15);border-radius:6px;color:#fff;font-size:13px;outline:none;width:50px;text-align:center}
        .btn-sm{padding:8px 14px;background:rgba(124,92,191,0.3);border:1px solid rgba(124,92,191,0.5);border-radius:6px;color:#e0e0ff;font-size:12px;cursor:pointer;transition:all .2s}
        .btn-sm:hover{background:rgba(124,92,191,0.5)}

        /* Empty state */
        .empty{text-align:center;padding:30px 10px;color:#555570}
        .empty p{font-size:13px;margin-bottom:12px}

        /* Device row */
        .dev-row{background:rgba(255,255,255,0.03);border:1px solid rgba(255,255,255,0.08);border-radius:10px;margin-bottom:8px;overflow:hidden}
        .dev-header{display:flex;align-items:center;padding:12px 14px;cursor:pointer;transition:background .2s}
        .dev-header:hover{background:rgba(255,255,255,0.04)}
        .dev-arrow{color:#7c5cbf;margin-right:10px;font-size:12px;transition:transform .2s}
        .dev-arrow.open{transform:rotate(90deg)}
        .dev-name{flex:1;font-size:14px;font-weight:500}
        .dev-id{color:#8888aa;font-size:12px;margin-right:8px}
        .dev-menu{color:#8888aa;font-size:16px;cursor:pointer;padding:0 4px;position:relative}

        /* Expanded controls */
        .dev-body{display:none;padding:12px 14px;border-top:1px solid rgba(255,255,255,0.06)}
        .dev-body.open{display:block}

        /* Toggle switch */
        .socket-row{display:flex;align-items:center;padding:8px 0}
        .socket-label{width:70px;font-size:13px;color:#b0b0cc}
        .toggle{position:relative;width:44px;height:24px;margin:0 12px}
        .toggle input{display:none}
        .toggle .slider{position:absolute;top:0;left:0;right:0;bottom:0;background:#444;border-radius:24px;cursor:pointer;transition:.3s}
        .toggle input:checked+.slider{background:#2ecc71}
        .toggle .slider:before{content:'';position:absolute;height:18px;width:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.3s}
        .toggle input:checked+.slider:before{transform:translateX(20px)}
        .socket-state{font-size:12px;font-weight:600;width:30px}
        .socket-current{flex:1;text-align:right;color:#8888aa;font-size:12px}

        /* Threshold row */
        .thr-row{display:flex;gap:6px;align-items:center;margin-top:8px;padding-top:8px;border-top:1px solid rgba(255,255,255,0.05)}
        .thr-label{color:#8888aa;font-size:12px}
        .thr-input{width:70px;padding:6px 8px;background:rgba(255,255,255,0.08);border:1px solid rgba(255,255,255,0.15);border-radius:6px;color:#fff;font-size:12px;outline:none}

        /* Add button */
        .btn-add{display:block;width:100%;padding:12px;background:transparent;border:1px dashed rgba(124,92,191,0.4);border-radius:10px;color:#7c5cbf;font-size:13px;cursor:pointer;transition:all .2s;text-align:center}
        .btn-add:hover{background:rgba(124,92,191,0.1);border-color:#7c5cbf}

        /* Modal */
        .modal-bg{display:none;position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.7);z-index:100;align-items:center;justify-content:center}
        .modal-bg.show{display:flex}
        .modal{background:#1a1a3e;border:1px solid rgba(255,255,255,0.15);border-radius:14px;padding:24px;width:90%;max-width:340px}
        .modal h3{color:#e0e0ff;font-size:16px;margin-bottom:16px}
        .modal input{width:100%;padding:10px 12px;background:rgba(255,255,255,0.08);border:1px solid rgba(255,255,255,0.15);border-radius:8px;color:#fff;font-size:14px;outline:none;margin-bottom:10px}
        .modal input:focus{border-color:#7c5cbf}
        .modal-btns{display:flex;gap:8px;margin-top:6px}
        .modal-btns button{flex:1;padding:10px;border:none;border-radius:8px;font-size:13px;cursor:pointer;font-weight:600}
        .btn-cancel{background:rgba(255,255,255,0.08);color:#b0b0cc}
        .btn-save{background:linear-gradient(135deg,#7c5cbf,#e94560);color:#fff}

        /* Context menu */
        .ctx-menu{display:none;position:absolute;right:0;top:100%;background:#252550;border:1px solid rgba(255,255,255,0.15);border-radius:8px;min-width:150px;z-index:50;overflow:hidden}
        .ctx-menu.show{display:block}
        .ctx-item{padding:10px 14px;font-size:12px;color:#b0b0cc;cursor:pointer;transition:background .2s}
        .ctx-item:hover{background:rgba(255,255,255,0.06);color:#e0e0ff}
        .ctx-item.danger{color:#e74c3c}
        .ctx-item.danger:hover{background:rgba(231,76,60,0.15)}

        /* Footer */
        .footer{text-align:center;margin-top:12px}
        .footer a{color:#7c5cbf;text-decoration:none;font-size:12px}
        .footer a:hover{color:#e0e0ff}

        /* Toast */
        .toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%) translateY(80px);background:rgba(78,204,163,0.9);color:#0f0c29;padding:8px 20px;border-radius:6px;font-size:12px;font-weight:600;opacity:0;transition:all .3s;pointer-events:none;z-index:200}
        .toast.show{transform:translateX(-50%) translateY(0);opacity:1}
        .toast.error{background:rgba(231,76,60,0.9);color:#fff}

        /* Config warning */
        .warn{background:rgba(241,196,15,0.1);border:1px solid rgba(241,196,15,0.3);border-radius:6px;padding:8px 10px;font-size:11px;color:#f1c40f;margin-bottom:10px}
    </style>
</head>
<body>
<div class="ctr">
    <div class="hdr">
        <h1>&#9889; Smart Outlet</h1>
        <div class="sub">Local Dashboard</div>
    </div>

    <!-- Master ID -->
    <div class="card">
        <div class="card-t">Master ID</div>
        <div class="master-row">
            <div class="master-show" id="masterShow">0x01</div>
            <span style="color:#8888aa;font-size:13px">0x</span>
            <input class="inp-sm" id="masterInput" maxlength="2" placeholder="01">
            <button class="btn-sm" onclick="setMasterID()">Set</button>
        </div>
    </div>

    <!-- Devices -->
    <div class="card">
        <div class="card-t">Devices</div>
        <div id="deviceList"></div>
        <button class="btn-add" onclick="showAddModal()">+ Add Device</button>
    </div>

    <div class="footer">
        <a href="/settings">&#9881; WiFi Settings</a>
    </div>
</div>

<!-- Add Device Modal -->
<div class="modal-bg" id="addModal">
    <div class="modal">
        <h3>Add New Device</h3>
        <input id="addName" placeholder="Device name (e.g. Garage Outlet)">
        <div style="display:flex;align-items:center;gap:6px">
            <span style="color:#8888aa">0x</span>
            <input id="addId" maxlength="2" placeholder="FE" style="width:60px">
        </div>
        <div class="modal-btns">
            <button class="btn-cancel" onclick="hideAddModal()">Cancel</button>
            <button class="btn-save" onclick="addDevice()">Save</button>
        </div>
    </div>
</div>

<!-- Rename Modal -->
<div class="modal-bg" id="renameModal">
    <div class="modal">
        <h3>Rename Device</h3>
        <input id="renameName" placeholder="New name">
        <input type="hidden" id="renameIdx">
        <div class="modal-btns">
            <button class="btn-cancel" onclick="hideRenameModal()">Cancel</button>
            <button class="btn-save" onclick="renameDevice()">Save</button>
        </div>
    </div>
</div>

<!-- Change ID Modal -->
<div class="modal-bg" id="changeIdModal">
    <div class="modal">
        <h3>Change Device ID</h3>
        <div class="warn">&#9888; The PIC must be in config mode (press RB3 button 3 times) before changing ID.</div>
        <div style="display:flex;align-items:center;gap:6px">
            <span style="color:#8888aa">New ID: 0x</span>
            <input id="changeIdVal" maxlength="2" placeholder="FE" style="width:60px">
        </div>
        <input type="hidden" id="changeIdIdx">
        <div class="modal-btns">
            <button class="btn-cancel" onclick="hideChangeIdModal()">Cancel</button>
            <button class="btn-save" onclick="changeDeviceId()">Send Command</button>
        </div>
    </div>
</div>

<!-- Toast -->
<div class="toast" id="toast"></div>

<script>
var devices=[];
var expanded=-1;
var pollTimer=null;
var ctxOpen=-1;

function toast(m,e){var t=document.getElementById('toast');t.textContent=m;t.className='toast show'+(e?' error':'');setTimeout(function(){t.className='toast'},2000)}
function api(method,path,cb){var x=new XMLHttpRequest();x.open(method,path,true);x.onload=function(){if(x.status===200){if(cb)cb(JSON.parse(x.responseText))}else{toast('Error: '+x.status,true)}};x.onerror=function(){toast('Connection failed',true)};x.send()}

// Load devices
function loadDevices(){
    api('GET','/api/devices',function(list){
        devices=list;
        render();
    });
}

// Render device list
function render(){
    var el=document.getElementById('deviceList');
    if(devices.length===0){
        el.innerHTML='<div class="empty"><p>No devices yet.<br>Tap "+" to add one.</p></div>';
        return;
    }
    var h='';
    for(var i=0;i<devices.length;i++){
        var d=devices[i];
        var isOpen=expanded===i;
        h+='<div class="dev-row">';
        h+='<div class="dev-header" onclick="toggleDevice('+i+')">';
        h+='<span class="dev-arrow'+(isOpen?' open':'')+'">&#9654;</span>';
        h+='<span class="dev-name">'+esc(d.name)+'</span>';
        h+='<span class="dev-id">'+d.id+'</span>';
        h+='<div class="dev-menu" onclick="event.stopPropagation();toggleCtx('+i+')">&vellip;';
        h+='<div class="ctx-menu'+(ctxOpen===i?' show':'')+'" id="ctx'+i+'">';
        h+='<div class="ctx-item" onclick="event.stopPropagation();showRenameModal('+i+')">Rename</div>';
        h+='<div class="ctx-item" onclick="event.stopPropagation();showChangeIdModal('+i+')">Change Device ID</div>';
        h+='<div class="ctx-item danger" onclick="event.stopPropagation();deleteDevice('+i+')">Delete</div>';
        h+='</div></div></div>';

        h+='<div class="dev-body'+(isOpen?' open':'')+'" id="body'+i+'">';
        // Socket A
        h+='<div class="socket-row">';
        h+='<span class="socket-label">Socket A</span>';
        h+='<label class="toggle"><input type="checkbox" id="relA'+i+'" onchange="toggleRelay('+i+',1,this.checked)"><span class="slider"></span></label>';
        h+='<span class="socket-state" id="stA'+i+'">--</span>';
        h+='<span class="socket-current" id="curA'+i+'">-- mA</span>';
        h+='</div>';
        // Socket B
        h+='<div class="socket-row">';
        h+='<span class="socket-label">Socket B</span>';
        h+='<label class="toggle"><input type="checkbox" id="relB'+i+'" onchange="toggleRelay('+i+',2,this.checked)"><span class="slider"></span></label>';
        h+='<span class="socket-state" id="stB'+i+'">--</span>';
        h+='<span class="socket-current" id="curB'+i+'">-- mA</span>';
        h+='</div>';
        // Threshold
        h+='<div class="thr-row">';
        h+='<span class="thr-label">Threshold:</span>';
        h+='<input class="thr-input" id="thr'+i+'" type="number" placeholder="3000">';
        h+='<span class="thr-label">mA</span>';
        h+='<button class="btn-sm" onclick="setThreshold('+i+')">Set</button>';
        h+='</div>';
        h+='</div></div>';
    }
    el.innerHTML=h;
}

function esc(s){var d=document.createElement('div');d.textContent=s;return d.innerHTML}

// Toggle expand/collapse
function toggleDevice(i){
    if(expanded===i){expanded=-1;stopPoll()}
    else{expanded=i;startPoll(i)}
    ctxOpen=-1;
    render();
}

// Context menu — pause polling when open
function toggleCtx(i){ctxOpen=ctxOpen===i?-1:i;if(ctxOpen>=0)stopPoll();else if(expanded>=0)startPoll(expanded);render()}
document.addEventListener('click',function(){if(ctxOpen>=0){ctxOpen=-1;render();if(expanded>=0)startPoll(expanded)}});

// Relay toggle
function toggleRelay(i,socket,on){
    api('POST','/api/relay?index='+i+'&socket='+socket+'&state='+(on?'on':'off'),function(){
        toast('Relay '+(socket===1?'A':'B')+' '+(on?'ON':'OFF'));
    });
}

// Threshold
function setThreshold(i){
    var v=document.getElementById('thr'+i).value;
    if(!v||v<=0){toast('Enter valid mA',true);return}
    api('POST','/api/threshold?index='+i+'&value='+v,function(){toast('Threshold: '+v+' mA')});
}

// Master ID
function setMasterID(){
    var v=document.getElementById('masterInput').value.trim();
    if(!v){toast('Enter master ID',true);return}
    if(devices.length===0){toast('Add devices first',true);return}
    stopPoll();
    toast('Setting Master ID... please wait');
    api('POST','/api/master?value='+v,function(r){
        document.getElementById('masterShow').textContent='0x'+v.toUpperCase();
        document.getElementById('masterInput').value='';
        if(r.success===r.total){toast('Master ID set on all '+r.total+' device(s)')}
        else{toast(r.success+'/'+r.total+' devices confirmed',true)}
        if(expanded>=0)startPoll(expanded);
    });
}

// Add device
function showAddModal(){document.getElementById('addModal').className='modal-bg show'}
function hideAddModal(){document.getElementById('addModal').className='modal-bg';document.getElementById('addName').value='';document.getElementById('addId').value=''}
function addDevice(){
    var n=document.getElementById('addName').value.trim();
    var id=document.getElementById('addId').value.trim();
    if(!n||!id){toast('Fill in both fields',true);return}
    api('POST','/api/devices/add?name='+encodeURIComponent(n)+'&id='+id,function(){hideAddModal();loadDevices();toast('Device added')});
}

// Rename
function showRenameModal(i){ctxOpen=-1;document.getElementById('renameIdx').value=i;document.getElementById('renameName').value=devices[i].name;document.getElementById('renameModal').className='modal-bg show';render()}
function hideRenameModal(){document.getElementById('renameModal').className='modal-bg'}
function renameDevice(){
    var i=document.getElementById('renameIdx').value;
    var n=document.getElementById('renameName').value.trim();
    if(!n){toast('Enter a name',true);return}
    api('POST','/api/devices/rename?index='+i+'&name='+encodeURIComponent(n),function(){hideRenameModal();loadDevices();toast('Renamed')});
}

// Change Device ID
function showChangeIdModal(i){ctxOpen=-1;document.getElementById('changeIdIdx').value=i;document.getElementById('changeIdModal').className='modal-bg show';render()}
function hideChangeIdModal(){document.getElementById('changeIdModal').className='modal-bg'}
function changeDeviceId(){
    var i=document.getElementById('changeIdIdx').value;
    var v=document.getElementById('changeIdVal').value.trim();
    if(!v){toast('Enter new ID',true);return}
    stopPoll();
    toast('Sending command... waiting for ACK');
    api('POST','/api/devices/changeId?index='+i+'&newId='+v,function(r){
        hideChangeIdModal();
        loadDevices();
        if(r.success){toast('Device ID changed successfully')}
        else{toast(r.reason||'Failed — no ACK',true)}
        if(expanded>=0)startPoll(expanded);
    });
}

// Delete
function deleteDevice(i){
    ctxOpen=-1;
    if(expanded===i){expanded=-1;stopPoll()}
    api('POST','/api/devices/delete?index='+i,function(){loadDevices();toast('Device deleted')});
}

// Polling
function startPoll(i){
    stopPoll();
    pollOnce(i);
    pollTimer=setInterval(function(){pollOnce(i)},2000);
}
function stopPoll(){if(pollTimer){clearInterval(pollTimer);pollTimer=null}}
function pollOnce(i){
    // Request fresh sensor read
    api('POST','/api/sensors?index='+i,function(){});
    // Get status
    setTimeout(function(){
        api('GET','/api/status?index='+i,function(s){
            // Relay A
            var cA=document.getElementById('relA'+i);
            var sA=document.getElementById('stA'+i);
            if(cA&&s.relayA>=0){cA.checked=s.relayA===1;sA.textContent=s.relayA?'ON':'OFF';sA.style.color=s.relayA?'#2ecc71':'#e74c3c'}
            // Relay B
            var cB=document.getElementById('relB'+i);
            var sB=document.getElementById('stB'+i);
            if(cB&&s.relayB>=0){cB.checked=s.relayB===1;sB.textContent=s.relayB?'ON':'OFF';sB.style.color=s.relayB?'#2ecc71':'#e74c3c'}
            // Current A
            var ca=document.getElementById('curA'+i);
            if(ca){ca.textContent=s.currentA>=0?(s.currentA+' mA'):'-- mA'}
            // Current B
            var cb=document.getElementById('curB'+i);
            if(cb){cb.textContent=s.currentB>=0?(s.currentB+' mA'):'-- mA'}
            // Master ID display
            if(s.masterID>=0){
                var hex=s.masterID.toString(16).toUpperCase();
                document.getElementById('masterShow').textContent='0x'+(hex.length<2?'0':'')+hex;
            }
        });
    },300);
}

// Init
loadDevices();
</script>
</body>
</html>
)rawliteral";

    return html;
}

// ════════════════════════════════════════════════════════════
//  HTML: SETTINGS PAGE
// ════════════════════════════════════════════════════════════

String Dashboard::_buildSettingsPage() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Settings</title>
    <style>
        *{margin:0;padding:0;box-sizing:border-box}
        body{font-family:'Segoe UI',Tahoma,sans-serif;background:linear-gradient(135deg,#0f0c29,#302b63,#24243e);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
        .card{background:rgba(255,255,255,0.05);backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,0.1);border-radius:20px;padding:40px 32px;width:100%;max-width:400px;box-shadow:0 8px 32px rgba(0,0,0,0.4)}
        h1{color:#e0e0ff;text-align:center;font-size:22px;font-weight:600;margin-bottom:6px}
        .subtitle{color:#8888aa;text-align:center;font-size:13px;margin-bottom:28px}
        .form-group{margin-bottom:18px}
        label{display:block;color:#b0b0cc;font-size:13px;font-weight:500;margin-bottom:6px}
        input[type="text"],input[type="password"],input[type="url"]{width:100%;padding:12px 16px;background:rgba(255,255,255,0.08);border:1px solid rgba(255,255,255,0.15);border-radius:10px;color:#fff;font-size:15px;outline:none}
        input:focus{border-color:#7c5cbf}
        input::placeholder{color:#666680}
        .btn{width:100%;padding:14px;background:linear-gradient(135deg,#7c5cbf,#e94560);border:none;border-radius:10px;color:#fff;font-size:16px;font-weight:600;cursor:pointer;margin-top:10px;transition:transform .2s}
        .btn:hover{transform:translateY(-1px)}
        .back-link{display:block;text-align:center;margin-top:18px;color:#7c5cbf;text-decoration:none;font-size:13px}
        .back-link:hover{color:#e0e0ff}
        .current{background:rgba(78,204,163,0.1);border:1px solid rgba(78,204,163,0.2);border-radius:8px;padding:12px;margin-bottom:24px;font-size:12px;color:#8888aa}
        .current strong{color:#4ecca3}
    </style>
</head>
<body>
    <div class="card">
        <h1>&#9881; WiFi Settings</h1>
        <p class="subtitle">Connect to your home network</p>
)rawliteral";

    if (_config.hasSavedConfig()) {
        html += "<div class=\"current\">";
        html += "Current: <strong>" + _config.getSSID() + "</strong>";
        if (_config.getServerUrl().length() > 0) {
            html += "<br>Server: <strong>" + _config.getServerUrl() + "</strong>";
        }
        html += "</div>";
    }

    html += R"rawliteral(
        <form action="/settings/save" method="POST">
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
                <input type="url" name="serverUrl" placeholder="http://your-server.com">
            </div>
            <button type="submit" class="btn">Save &amp; Connect</button>
        </form>
        <a href="/dashboard" class="back-link">&larr; Back to Dashboard</a>
    </div>
</body>
</html>
)rawliteral";

    return html;
}
