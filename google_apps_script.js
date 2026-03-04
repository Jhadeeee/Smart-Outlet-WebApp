// ============================================================
// Smart Outlet — Google Sheets Data Sync (Apps Script)
// ============================================================
// 
// HOW TO USE:
// 1. Open your Google Sheet
// 2. Go to Extensions → Apps Script
// 3. Delete any existing code and paste this entire file
// 4. Update SERVER_URL and API_KEY below
// 5. Click the Save icon (or Ctrl+S)
// 6. Select "fetchAllData" from the function dropdown, then click Run
// 7. Authorize the script when prompted
// 8. Use the custom menu "Smart Outlet → Refresh Data" to update anytime
// 9. (Optional) Run "createAutoRefreshTrigger" to enable auto-refresh every 1 minute
//
// ============================================================

// ╔══════════════════════════════════════════════════════════╗
// ║  CONFIGURATION — UPDATE THESE VALUES                    ║
// ╚══════════════════════════════════════════════════════════╝

const SERVER_URL = "https://decurrently-schismless-dot.ngrok-free.dev";  // ngrok tunnel to localhost:8000
const API_KEY = "smartoutlet-sheets-export-2026";             // Matches SHEETS_API_KEY in .env
const DAYS = 30;                              // How many days of data to fetch

// Outlet device IDs — must match registered outlets in Django
const OUTLETS = [
    { deviceId: "FE", sheetName: "Outlet 1 (0xFE)" },
    { deviceId: "02", sheetName: "Outlet 2 (0x02)" },
    { deviceId: "03", sheetName: "Outlet 3 (0x03)" },
];

const BREAKER_SHEET_NAME = "Main Breaker";


// ╔══════════════════════════════════════════════════════════╗
// ║  CUSTOM MENU                                            ║
// ╚══════════════════════════════════════════════════════════╝

function onOpen() {
    SpreadsheetApp.getUi()
        .createMenu("⚡ Smart Outlet")
        .addItem("🔄 Refresh Data", "fetchAllData")
        .addSeparator()
        .addItem("⏱️ Enable Auto-Refresh (1 min)", "createAutoRefreshTrigger")
        .addItem("🛑 Disable Auto-Refresh", "removeAutoRefreshTrigger")
        .addToUi();
}


// ╔══════════════════════════════════════════════════════════╗
// ║  MAIN FUNCTION — FETCH ALL DATA                         ║
// ╚══════════════════════════════════════════════════════════╝

function fetchAllData() {
    var ss = SpreadsheetApp.getActiveSpreadsheet();

    // Fetch data from Django API
    var url = SERVER_URL + "/api/export/sheets/?key=" + API_KEY + "&days=" + DAYS;

    var response;
    try {
        response = UrlFetchApp.fetch(url, {
            muteHttpExceptions: true,
            headers: {
                "Accept": "application/json",
                "ngrok-skip-browser-warning": "true"
            }
        });
    } catch (e) {
        try {
            SpreadsheetApp.getUi().alert("❌ Connection Error\n\nCould not reach the server:\n" + SERVER_URL + "\n\nError: " + e.message);
        } catch (uiErr) {
            Logger.log("Connection error: " + e.message);
        }
        return;
    }

    var code = response.getResponseCode();
    if (code !== 200) {
        try {
            SpreadsheetApp.getUi().alert("❌ API Error (HTTP " + code + ")\n\n" + response.getContentText());
        } catch (uiErr) {
            Logger.log("API Error (HTTP " + code + "): " + response.getContentText());
        }
        return;
    }

    var jsonData = JSON.parse(response.getContentText());

    if (!jsonData.success) {
        try {
            SpreadsheetApp.getUi().alert("❌ API returned error:\n" + (jsonData.message || "Unknown error"));
        } catch (uiErr) {
            Logger.log("API returned error: " + (jsonData.message || "Unknown error"));
        }
        return;
    }

    // ---- Populate Outlet Sheets ----
    for (var i = 0; i < OUTLETS.length; i++) {
        var outletConfig = OUTLETS[i];
        var outletData = jsonData.outlets[outletConfig.deviceId];

        var sheet = getOrCreateSheet(ss, outletConfig.sheetName);
        populateOutletSheet(sheet, outletConfig, outletData);
    }

    // ---- Populate Main Breaker Sheet ----
    var breakerSheet = getOrCreateSheet(ss, BREAKER_SHEET_NAME);
    populateBreakerSheet(breakerSheet, jsonData.breaker);

    // Show confirmation (only when run manually)
    try {
        SpreadsheetApp.getUi().alert("✅ Data refreshed successfully!");
    } catch (e) {
        // Triggered by timer — no UI available, that's fine
    }
}


// ╔══════════════════════════════════════════════════════════╗
// ║  POPULATE OUTLET SHEET                                  ║
// ╚══════════════════════════════════════════════════════════╝

function populateOutletSheet(sheet, outletConfig, outletData) {
    sheet.clearContents();

    // Header row
    var headers = ["Timestamp", "Current A (mA)", "Current B (mA)", "Threshold (mA)", "Overload"];
    sheet.getRange(1, 1, 1, headers.length).setValues([headers]);

    // Style header
    var headerRange = sheet.getRange(1, 1, 1, headers.length);
    headerRange.setFontWeight("bold");
    headerRange.setBackground("#4285F4");
    headerRange.setFontColor("#FFFFFF");
    headerRange.setHorizontalAlignment("center");

    // Data rows
    if (!outletData || !outletData.readings || outletData.readings.length === 0) {
        sheet.getRange(2, 1).setValue("No data available for " + outletConfig.sheetName);
        autoResizeColumns(sheet, headers.length);
        return;
    }

    var readings = outletData.readings;
    var rows = [];

    for (var i = 0; i < readings.length; i++) {
        var r = readings[i];
        rows.push([
            r.timestamp,
            r.current_a,
            r.current_b,
            r.threshold,
            r.is_overload ? "YES ⚠️" : "No"
        ]);
    }

    if (rows.length > 0) {
        sheet.getRange(2, 1, rows.length, headers.length).setValues(rows);

        // Alternate row colors for readability
        for (var j = 0; j < rows.length; j++) {
            var rowRange = sheet.getRange(j + 2, 1, 1, headers.length);
            if (j % 2 === 0) {
                rowRange.setBackground("#F8F9FA");
            } else {
                rowRange.setBackground("#FFFFFF");
            }

            // Highlight overload rows in red
            if (rows[j][4] === "YES ⚠️") {
                rowRange.setBackground("#FFCDD2");
                rowRange.setFontColor("#B71C1C");
            }
        }
    }

    // Number formatting for current columns
    if (rows.length > 0) {
        sheet.getRange(2, 2, rows.length, 3).setNumberFormat("#,##0");
    }

    autoResizeColumns(sheet, headers.length);

    // Freeze header row
    sheet.setFrozenRows(1);
}


// ╔══════════════════════════════════════════════════════════╗
// ║  POPULATE BREAKER SHEET                                 ║
// ╚══════════════════════════════════════════════════════════╝

function populateBreakerSheet(sheet, breakerData) {
    sheet.clearContents();

    // Header row
    var headers = ["Timestamp", "CCU ID", "Current (mA)", "Threshold (mA)"];
    sheet.getRange(1, 1, 1, headers.length).setValues([headers]);

    // Style header
    var headerRange = sheet.getRange(1, 1, 1, headers.length);
    headerRange.setFontWeight("bold");
    headerRange.setBackground("#34A853");
    headerRange.setFontColor("#FFFFFF");
    headerRange.setHorizontalAlignment("center");

    // Data rows
    if (!breakerData || breakerData.length === 0) {
        sheet.getRange(2, 1).setValue("No breaker data available");
        autoResizeColumns(sheet, headers.length);
        return;
    }

    var rows = [];

    for (var i = 0; i < breakerData.length; i++) {
        var r = breakerData[i];
        rows.push([
            r.timestamp,
            r.ccu_id,
            r.current_ma,
            r.threshold
        ]);
    }

    if (rows.length > 0) {
        sheet.getRange(2, 1, rows.length, headers.length).setValues(rows);

        // Alternate row colors
        for (var j = 0; j < rows.length; j++) {
            var rowRange = sheet.getRange(j + 2, 1, 1, headers.length);
            if (j % 2 === 0) {
                rowRange.setBackground("#F8F9FA");
            } else {
                rowRange.setBackground("#FFFFFF");
            }
        }

        // Number formatting
        sheet.getRange(2, 3, rows.length, 2).setNumberFormat("#,##0");
    }

    autoResizeColumns(sheet, headers.length);

    // Freeze header row
    sheet.setFrozenRows(1);
}


// ╔══════════════════════════════════════════════════════════╗
// ║  HELPER FUNCTIONS                                       ║
// ╚══════════════════════════════════════════════════════════╝

/**
 * Get an existing sheet by name, or create it if it doesn't exist.
 */
function getOrCreateSheet(ss, name) {
    var sheet = ss.getSheetByName(name);
    if (!sheet) {
        sheet = ss.insertSheet(name);
    }
    return sheet;
}

/**
 * Auto-resize all columns for readability.
 */
function autoResizeColumns(sheet, numCols) {
    for (var i = 1; i <= numCols; i++) {
        sheet.autoResizeColumn(i);
    }
}


// ╔══════════════════════════════════════════════════════════╗
// ║  AUTO-REFRESH TRIGGER (Every 1 Minute)                  ║
// ╚══════════════════════════════════════════════════════════╝

/**
 * Creates a time-driven trigger that runs fetchAllData every 1 minute.
 * Run this once to enable automatic refreshing.
 */
function createAutoRefreshTrigger() {
    // Remove any existing triggers first to avoid duplicates
    removeAutoRefreshTrigger();

    ScriptApp.newTrigger("fetchAllData")
        .timeBased()
        .everyMinutes(1)
        .create();

    try {
        SpreadsheetApp.getUi().alert("✅ Auto-refresh enabled!\n\nData will update every 1 minute automatically.");
    } catch (e) {
        Logger.log("Auto-refresh trigger created successfully.");
    }
}

/**
 * Removes the auto-refresh trigger.
 */
function removeAutoRefreshTrigger() {
    var triggers = ScriptApp.getProjectTriggers();
    for (var i = 0; i < triggers.length; i++) {
        if (triggers[i].getHandlerFunction() === "fetchAllData") {
            ScriptApp.deleteTrigger(triggers[i]);
        }
    }

    try {
        SpreadsheetApp.getUi().alert("🛑 Auto-refresh disabled.");
    } catch (e) {
        // Called programmatically (no UI)
    }
}
