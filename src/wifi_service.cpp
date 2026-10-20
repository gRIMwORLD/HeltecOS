#include "wifi_service.h"
#include "board_hal.h"
#include "gui_manager.h"

// Beautiful Dark-Mode Responsive HTML / CSS / JS UI
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>HeltecOS Dashboard</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --border: #334155;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --accent: #38bdf8;
      --accent-green: #4ade80;
      --accent-orange: #fb923c;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 20px; display: flex; justify-content: center; }
    .container { width: 100%; max-width: 720px; }
    header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; padding-bottom: 16px; border-bottom: 1px solid var(--border); }
    h1 { font-size: 24px; color: var(--accent); }
    .badge { background: #0369a1; padding: 4px 10px; border-radius: 999px; font-size: 12px; font-weight: bold; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin-bottom: 24px; }
    .card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 18px; }
    .card-title { font-size: 13px; text-transform: uppercase; color: var(--text-muted); letter-spacing: 0.5px; margin-bottom: 8px; }
    .card-val { font-size: 32px; font-weight: 700; color: var(--text); }
    .card-sub { font-size: 13px; color: var(--text-muted); margin-top: 4px; }
    .btn-group { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 12px; }
    button { background: #2563eb; color: #fff; border: none; padding: 10px 16px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: 0.2s; }
    button:hover { background: #1d4ed8; }
    button.btn-sec { background: #475569; }
    button.btn-sec:hover { background: #334155; }
    .live-dot { display: inline-block; width: 8px; height: 8px; background: var(--accent-green); border-radius: 50%; margin-right: 6px; animation: pulse 1.5s infinite; }
    @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.3; } 100% { opacity: 1; } }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div>
        <h1>HeltecOS Dashboard</h1>
        <div style="font-size: 13px; color: var(--text-muted); margin-top: 4px;">
          <span class="live-dot"></span>Connected to <span id="ssid">--</span>
        </div>
      </div>
      <div>
        <span class="badge" id="clock">--:--:--</span>
      </div>
    </header>

    <div class="grid">
      <div class="card">
        <div class="card-title">Temperature</div>
        <div class="card-val" id="temp">--.- &deg;C</div>
        <div class="card-sub" id="temp-f">--.- &deg;F</div>
      </div>
      <div class="card">
        <div class="card-title">Relative Humidity</div>
        <div class="card-val" id="hum">--.- %</div>
        <div class="card-sub">Air Moisture Content</div>
      </div>
      <div class="card">
        <div class="card-title">Barometric Pressure</div>
        <div class="card-val" id="pres">---- hPa</div>
        <div class="card-sub" id="alt">Alt: ---- m</div>
      </div>
      <div class="card">
        <div class="card-title">Battery Status</div>
        <div class="card-val" id="bat-pct">-- %</div>
        <div class="card-sub" id="bat-v">--.- V &bull; Normal</div>
      </div>
    </div>

    <div class="card" style="margin-bottom: 24px;">
      <div class="card-title">Hardware Controls</div>
      <div class="btn-group">
        <button onclick="sendAction('led_toggle')">Toggle User LED</button>
        <button class="btn-sec" onclick="sendAction('screen_next')">Next OLED Screen</button>
        <button class="btn-sec" onclick="sendAction('portal')">Setup Hotspot (AP)</button>
        <button class="btn-sec" style="background: #dc2626;" onclick="if(confirm('Reboot device?')) sendAction('reboot')">Reboot</button>
      </div>
    </div>

    <div style="text-align: center; font-size: 12px; color: var(--text-muted);">
      Heltec WiFi Kit 32 (V3) &bull; ESP32-S3 Dual-Core FreeRTOS &bull; <span id="heap">--</span> KB Free RAM
    </div>
  </div>

  <script>
    async function updateData() {
      try {
        const res = await fetch('/api/telemetry');
        const data = await res.json();
        document.getElementById('temp').innerHTML = data.temp_c.toFixed(1) + ' &deg;C';
        document.getElementById('temp-f').innerHTML = ((data.temp_c * 9/5) + 32).toFixed(1) + ' &deg;F';
        document.getElementById('hum').innerText = data.humidity.toFixed(1) + ' %';
        document.getElementById('pres').innerText = data.pressure.toFixed(1) + ' hPa';
        document.getElementById('alt').innerText = 'Alt: ' + data.altitude.toFixed(0) + ' m';
        document.getElementById('bat-pct').innerText = data.battery_pct + ' %';
        const chgLabel = data.charging ? '<span style="color: #4ade80;">Charging</span>' : 'Discharging';
        document.getElementById('bat-v').innerHTML = data.battery_v.toFixed(2) + ' V &bull; ' + chgLabel;
        document.getElementById('ssid').innerText = data.ssid;
        document.getElementById('clock').innerText = data.time;
        document.getElementById('heap').innerText = Math.round(data.free_heap / 1024);
      } catch (e) {
        console.error(e);
      }
    }
    async function sendAction(act) {
      await fetch('/api/action?cmd=' + act);
      setTimeout(updateData, 200);
    }
    setInterval(updateData, 1500);
    updateData();
  </script>
</body>
</html>
)rawliteral";

WifiService& WifiService::instance() {
    static WifiService ws;
    return ws;
}

WifiService::WifiService() 
    : server(80), state(WIFI_DISCONNECTED), time_synced(false),
      tz_offset_hours(0), last_ntp_check(0), last_wifi_status_check(0) {}

void WifiService::init() {
    // Non-blocking WiFiManager configuration
    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(180); // 3 minutes timeout if opened
    wm.setDarkMode(true);

    // Try auto-connect to stored credentials in background
    Serial.println("[WIFI] Initializing WiFiManager...");
    if (wm.autoConnect("HeltecOS-Setup")) {
        state = WIFI_CONNECTED;
        Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        syncNTP();
        setupWebServer();
    } else {
        // If not connected, WiFiManager automatically starts non-blocking config portal
        state = WIFI_PORTAL_ACTIVE;
        Serial.println("[WIFI] AutoConnect failed or no credentials. Hotspot 'HeltecOS-Setup' active.");
    }
}

void WifiService::startPortal() {
    state = WIFI_PORTAL_ACTIVE;
    wm.startConfigPortal("HeltecOS-Setup");
    Serial.println("[WIFI] Captive portal 'HeltecOS-Setup' started at 192.168.4.1");
}

void WifiService::resetSettings() {
    wm.resetSettings();
    Serial.println("[WIFI] Stored WiFi credentials cleared from NVS.");
    delay(200);
    esp_restart();
}

void WifiService::process() {
    // Process WiFiManager background state machine
    wm.process();

    uint32_t now = millis();

    // Check WiFi connection status periodically
    if (now - last_wifi_status_check > 2000) {
        last_wifi_status_check = now;
        if (WiFi.status() == WL_CONNECTED) {
            if (state != WIFI_CONNECTED) {
                state = WIFI_CONNECTED;
                syncNTP();
                setupWebServer();
            }
        } else if (wm.getConfigPortalActive()) {
            state = WIFI_PORTAL_ACTIVE;
        } else {
            state = WIFI_DISCONNECTED;
        }
    }

    // Process HTTP Web Server if connected
    if (state == WIFI_CONNECTED) {
        server.handleClient();

        // Refresh NTP sync once every hour
        if (now - last_ntp_check > 3600000) {
            syncNTP();
        }
    }
}

void WifiService::setupWebServer() {
    static bool server_started = false;
    if (server_started) return;

    // mDNS http://heltecos.local
    if (MDNS.begin("heltecos")) {
        Serial.println("[WIFI] mDNS responder started: http://heltecos.local");
    }

    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/api/telemetry", HTTP_GET, [this]() { handleApiTelemetry(); });
    server.on("/api/action", HTTP_GET, [this]() { handleApiAction(); });

    server.begin();
    server_started = true;
    Serial.printf("[WIFI] Web Dashboard live at: http://%s\n", WiFi.localIP().toString().c_str());
}

void WifiService::handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void WifiService::handleApiTelemetry() {
    const TelemetryData& t = BoardHAL::instance().getTelemetry();

    char json[256];
    snprintf(json, sizeof(json),
        "{\"temp_c\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"altitude\":%.1f,"
        "\"battery_v\":%.2f,\"battery_pct\":%d,\"charging\":%s,"
        "\"free_heap\":%lu,\"ssid\":\"%s\",\"time\":\"%s\"}",
        t.temperature_c, t.humidity_pct, t.pressure_hpa, t.altitude_m,
        t.vbat_volts, t.battery_pct, t.is_charging ? "true" : "false",
        (unsigned long)t.free_heap, WiFi.SSID().c_str(), getTimeString().c_str()
    );

    server.send(200, "application/json", json);
}

void WifiService::handleApiAction() {
    if (server.hasArg("cmd")) {
        String cmd = server.arg("cmd");
        if (cmd == "led_toggle") {
            BoardHAL::instance().toggleLED();
        } else if (cmd == "screen_next") {
            GuiManager::instance().nextScreen();
        } else if (cmd == "portal") {
            startPortal();
        } else if (cmd == "reboot") {
            server.send(200, "text/plain", "Rebooting...");
            delay(200);
            esp_restart();
            return;
        }
    }
    server.send(200, "text/plain", "OK");
}

void WifiService::syncNTP() {
    configTime(tz_offset_hours * 3600, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 2000)) {
        time_synced = true;
        last_ntp_check = millis();
        Serial.printf("[NTP] Time synchronized: %02d:%02d:%02d\n", 
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        time_synced = false;
    }
}

bool WifiService::isTimeSynced() const {
    return time_synced;
}

String WifiService::getTimeString() const {
    if (!time_synced) {
        // Fallback to elapsed uptime
        uint32_t s = millis() / 1000;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (s / 3600) % 24, (s % 3600) / 60, s % 60);
        return String(buf);
    }
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        return String(buf);
    }
    return String("--:--:--");
}

String WifiService::getDateString() const {
    if (!time_synced) return String("----/--/--");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        return String(buf);
    }
    return String("----/--/--");
}

void WifiService::setTimezoneOffset(int hours) {
    tz_offset_hours = hours;
    syncNTP();
}

int WifiService::getTimezoneOffset() const {
    return tz_offset_hours;
}

WifiState WifiService::getState() const {
    return state;
}

bool WifiService::isConnected() const {
    return state == WIFI_CONNECTED;
}

bool WifiService::isPortalActive() const {
    return state == WIFI_PORTAL_ACTIVE;
}

String WifiService::getSSID() const {
    return (state == WIFI_CONNECTED) ? WiFi.SSID() : String("");
}

String WifiService::getIP() const {
    if (state == WIFI_CONNECTED) {
        return WiFi.localIP().toString();
    } else if (state == WIFI_PORTAL_ACTIVE) {
        return String("192.168.4.1");
    }
    return String("0.0.0.0");
}

int WifiService::getRSSI() const {
    return (state == WIFI_CONNECTED) ? WiFi.RSSI() : 0;
}
