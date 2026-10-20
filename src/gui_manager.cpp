#include "gui_manager.h"
#include "wifi_service.h"
#include <WiFi.h>

GuiManager& GuiManager::instance() {
    static GuiManager gm;
    return gm;
}

GuiManager::GuiManager() 
    : u8g2(U8G2_R0, PIN_OLED_SCL, PIN_OLED_SDA, PIN_OLED_RST),
      current_screen(SCREEN_DASHBOARD),
      last_frame_time(0) {}

void GuiManager::init() {
    Serial.println("[GUI] Initializing OLED display via U8g2...");
    u8g2.begin();
    u8g2.enableUTF8Print();

    Serial.println("[GUI] Drawing splash screen...");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB12_tr);
    u8g2.drawStr(12, 28, "HeltecOS");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(16, 44, "v0.1.0 Ready");
    u8g2.drawFrame(14, 52, 100, 6);
    u8g2.drawBox(16, 54, 96, 2);
    u8g2.sendBuffer();
    Serial.println("[GUI] OLED Display Active!");
}

void GuiManager::nextScreen() {
    current_screen = static_cast<GuiScreen>((current_screen + 1) % SCREEN_COUNT);
}

void GuiManager::setScreen(GuiScreen screen) {
    if (screen >= 0 && screen < SCREEN_COUNT) {
        current_screen = screen;
    }
}

GuiScreen GuiManager::getScreen() const {
    return current_screen;
}

void GuiManager::render() {
    const TelemetryData& t = BoardHAL::instance().getTelemetry();

    u8g2.clearBuffer();
    drawStatusBar(t);

    switch (current_screen) {
        case SCREEN_DASHBOARD:
            drawDashboard(t);
            break;
        case SCREEN_ENV_SENSOR:
            drawEnvSensor(t);
            break;
        case SCREEN_WIFI:
            drawWiFiScreen(t);
            break;
        case SCREEN_TASKS:
            drawTasksScreen(t);
            break;
        case SCREEN_ABOUT:
            drawAboutScreen(t);
            break;
        default:
            drawDashboard(t);
            break;
    }

    u8g2.sendBuffer();
}

void GuiManager::drawStatusBar(const TelemetryData& t) {
    u8g2.setFont(u8g2_font_5x7_tf);

    // Screen title / index indicator on the left
    char page_str[32];
    snprintf(page_str, sizeof(page_str), "PAGE %d/%d", current_screen + 1, SCREEN_COUNT);
    u8g2.drawStr(0, 7, page_str);

    // WiFi status or Clock in middle
    const WifiService& ws = WifiService::instance();
    if (ws.isConnected()) {
        String clock_str = ws.getTimeString();
        u8g2.drawStr(50, 7, clock_str.c_str());
    } else if (ws.isPortalActive()) {
        u8g2.drawStr(48, 7, "SETUP-AP");
    } else {
        u8g2.drawStr(52, 7, "WiFi:--");
    }

    // Battery percentage on right
    char bat_str[16];
    if (t.is_full) {
        snprintf(bat_str, sizeof(bat_str), "FULL");
    } else if (t.is_charging) {
        snprintf(bat_str, sizeof(bat_str), "CHG %d%%", t.battery_pct);
    } else {
        snprintf(bat_str, sizeof(bat_str), "%d%%", t.battery_pct);
    }
    int w = u8g2.getStrWidth(bat_str);
    u8g2.drawStr(128 - w, 7, bat_str);

    // Separator line
    u8g2.drawHLine(0, 9, 128);
}

void GuiManager::drawDashboard(const TelemetryData& t) {
    u8g2.setFont(u8g2_font_6x10_tf);

    // Uptime
    uint32_t s = t.uptime_sec;
    int h = s / 3600;
    int m = (s % 3600) / 60;
    int sec = s % 60;
    char up_str[32];
    snprintf(up_str, sizeof(up_str), "Up: %02d:%02d:%02d", h, m, sec);
    u8g2.drawStr(0, 21, up_str);

    // Battery voltage & capacity
    char bat_str[32];
    snprintf(bat_str, sizeof(bat_str), "Bat: %.2fV (%d%%)", t.vbat_volts, t.battery_pct);
    u8g2.drawStr(0, 32, bat_str);

    // Live power consumption & current
    char pwr_str[32];
    if (t.is_charging) {
        snprintf(pwr_str, sizeof(pwr_str), "Chg: +%.1fW (+%dmA)", t.power_mw / 1000.0f, (int)t.current_ma);
    } else {
        snprintf(pwr_str, sizeof(pwr_str), "Pwr: ~%dmW (%dmA)", (int)t.power_mw, (int)t.current_ma);
    }
    u8g2.drawStr(0, 43, pwr_str);

    // Time remaining / time to full
    char rem_str[32];
    int rem_h = (int)t.time_remaining_hours;
    int rem_m = (int)((t.time_remaining_hours - rem_h) * 60.0f);
    if (t.is_full) {
        snprintf(rem_str, sizeof(rem_str), "State: 100%% Charged");
    } else if (t.is_charging) {
        snprintf(rem_str, sizeof(rem_str), "Full in: ~%dh %02dm", rem_h, rem_m);
    } else {
        snprintf(rem_str, sizeof(rem_str), "Rem: ~%dh %02dm left", rem_h, rem_m);
    }
    u8g2.drawStr(0, 54, rem_str);

    // Bottom line (Free RAM & CPU)
    u8g2.setFont(u8g2_font_5x7_tf);
    char mem_str[32];
    snprintf(mem_str, sizeof(mem_str), "RAM:%luKB  ESP32-S3", (unsigned long)(t.free_heap / 1024));
    u8g2.drawStr(0, 63, mem_str);
}

void GuiManager::drawEnvSensor(const TelemetryData& t) {
    if (!t.sensor_present) {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(4, 25, "Sensor Offline");
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(0, 38, "Connect BME/BMP280:");
        u8g2.drawStr(0, 49, "VIN->3V3   GND->GND");
        u8g2.drawStr(0, 60, "SDA->Pin 4 SCL->Pin 5");
        return;
    }

    // Header label & Live Pulse Indicator
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 19, t.is_bme ? "BME280 SENSOR" : "BMP280 BAROMETER");

    // Live heartbeat indicator on top right of the sensor page
    if ((t.sensor_sample_count % 2) == 0) {
        u8g2.drawDisc(122, 16, 2); // Filled dot
    } else {
        u8g2.drawCircle(122, 16, 2); // Hollow circle
    }

    // Big Temperature
    u8g2.setFont(u8g2_font_helvB12_tf);
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.1f \xb0 C", t.temperature_c);
    u8g2.drawStr(4, 37, temp_str);

    // Humidity / Pressure / Altitude
    u8g2.setFont(u8g2_font_6x10_tf);
    char line2[32];
    if (t.is_bme) {
        snprintf(line2, sizeof(line2), "Hum:  %.1f %%", t.humidity_pct);
        u8g2.drawStr(4, 50, line2);
    } else {
        snprintf(line2, sizeof(line2), "Pres: %.1f hPa", t.pressure_hpa);
        u8g2.drawStr(4, 50, line2);
    }

    char line3[32];
    snprintf(line3, sizeof(line3), "%.0fhPa #%lu", t.pressure_hpa, (unsigned long)t.sensor_sample_count);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(4, 62, line3);
}

void GuiManager::drawWiFiScreen(const TelemetryData& t) {
    const WifiService& ws = WifiService::instance();

    u8g2.setFont(u8g2_font_6x10_tf);

    if (ws.isConnected()) {
        u8g2.drawStr(0, 21, "WiFi: Connected");

        u8g2.setFont(u8g2_font_5x7_tf);
        char ssid_str[32];
        snprintf(ssid_str, sizeof(ssid_str), "SSID: %s", ws.getSSID().c_str());
        u8g2.drawStr(0, 32, ssid_str);

        char ip_str[32];
        snprintf(ip_str, sizeof(ip_str), "IP:   %s", ws.getIP().c_str());
        u8g2.drawStr(0, 42, ip_str);

        u8g2.drawStr(0, 52, "Web:  http://heltecos.local");

        char rssi_str[32];
        snprintf(rssi_str, sizeof(rssi_str), "RSSI: %d dBm  Clock: OK", ws.getRSSI());
        u8g2.drawStr(0, 62, rssi_str);
    } else if (ws.isPortalActive()) {
        u8g2.drawStr(0, 21, "SETUP HOTSPOT ACTIVE");

        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(0, 33, "SSID: HeltecOS-Setup");
        u8g2.drawStr(0, 43, "IP:   192.168.4.1");
        u8g2.drawStr(0, 54, "Connect phone/laptop");
        u8g2.drawStr(0, 63, "to configure Wi-Fi");
    } else {
        u8g2.drawStr(0, 21, "WiFi: Disconnected");

        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(0, 34, "No saved Wi-Fi found");
        u8g2.drawStr(0, 46, "In CLI, type:");
        u8g2.drawStr(0, 58, "wifi portal");
    }
}

void GuiManager::drawTasksScreen(const TelemetryData& t) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 20, "TASK         PRIO CORE");
    u8g2.drawHLine(0, 22, 128);

    u8g2.drawStr(0, 32, "GUI_Task     1    1");
    u8g2.drawStr(0, 42, "Sensor_Task  1    1");
    u8g2.drawStr(0, 52, "CLI_Task     1    0");
    u8g2.drawStr(0, 62, "IDLE0/1      0    0/1");
}

void GuiManager::drawAboutScreen(const TelemetryData& t) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 22, "HeltecOS v0.1.0");

    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(0, 34, "Board: Heltec WiFi Kit 32 V3");
    u8g2.drawStr(0, 44, "SoC:   ESP32-S3FN8 Dual Core");
    u8g2.drawStr(0, 54, "USB:   COM7 (115200 Baud)");
    u8g2.drawStr(0, 63, "Click PRG to switch screen");
}
