#include "cli_shell.h"
#include "board_hal.h"
#include "gui_manager.h"
#include "wifi_service.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_chip_info.h>

CliShell& CliShell::instance() {
    static CliShell shell;
    return shell;
}

CliShell::CliShell() {
    input_buffer.reserve(128);
}

void CliShell::init() {
    Serial.begin(115200, SERIAL_8N1, 44, 43);
    // Give serial a moment to settle
    delay(200);
    printBanner();
    printPrompt();
}

void CliShell::printBanner() {
    Serial.println();
    Serial.println("\033[1;36m===================================================\033[0m");
    Serial.println("\033[1;32m   _   _      _ _             ____   _____ \033[0m");
    Serial.println("\033[1;32m  | | | | ___| | |_ ___  ___ / ___| / ____|\033[0m");
    Serial.println("\033[1;32m  | |_| |/ _ \\ | __/ _ \\/ __| |  _ | (___  \033[0m");
    Serial.println("\033[1;32m  |  _  |  __/ | ||  __/ (__| |_| | \\___ \\ \033[0m");
    Serial.println("\033[1;32m  |_| |_|\\___|_|\\__\\___|\\___|\\____| ____) |\033[0m");
    Serial.println("\033[1;36m===================================================\033[0m");
    Serial.println("  Heltec WiFi Kit 32 (V3) Embedded Micro-OS v0.1.0");
    Serial.println("  Type \033[1;33mhelp\033[0m for available commands.");
    Serial.println("\033[1;36m===================================================\033[0m");
    Serial.println();
}

void CliShell::printPrompt() {
    Serial.print("\033[1;32mHeltecOS\033[0m:\033[1;34m~/\033[0m$ ");
}

void CliShell::process() {
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (c == '\r') {
            continue; // Ignore CR, wait for LF
        } else if (c == '\n') {
            Serial.println();
            executeCommand(input_buffer);
            input_buffer = "";
            printPrompt();
        } else if (c == '\b' || c == 127) {
            // Handle Backspace
            if (input_buffer.length() > 0) {
                input_buffer.remove(input_buffer.length() - 1);
                Serial.print("\b \b");
            }
        } else if (c >= 32 && c <= 126) {
            // Printable character
            if (input_buffer.length() < 120) {
                input_buffer += c;
                Serial.write(c);
            }
        }
    }
}

void CliShell::executeCommand(const String& cmd_line) {
    String line = cmd_line;
    line.trim();
    if (line.length() == 0) return;

    int space_idx = line.indexOf(' ');
    String cmd = (space_idx == -1) ? line : line.substring(0, space_idx);
    String arg = (space_idx == -1) ? "" : line.substring(space_idx + 1);
    arg.trim();
    cmd.toLowerCase();

    if (cmd == "help") {
        cmdHelp();
    } else if (cmd == "sysinfo") {
        cmdSysinfo();
    } else if (cmd == "sensor") {
        cmdSensor();
    } else if (cmd == "battery") {
        cmdBattery();
    } else if (cmd == "i2c" && (arg == "scan" || arg == "")) {
        cmdI2cScan();
    } else if (cmd == "ps") {
        cmdPs();
    } else if (cmd == "prg" || cmd == "btn" || cmd == "click") {
        cmdScreen("");
    } else if (cmd == "offset") {
        cmdOffset(arg);
    } else if (cmd == "screen") {
        cmdScreen(arg);
    } else if (cmd == "led") {
        cmdLed(arg);
    } else if (cmd == "wifi") {
        if (arg == "scan") {
            cmdWifiScan();
        } else if (arg == "portal" || arg == "setup") {
            WifiService::instance().startPortal();
            GuiManager::instance().setScreen(SCREEN_WIFI);
            Serial.println("Captive Portal Hotspot 'HeltecOS-Setup' is now active at 192.168.4.1!");
            Serial.println("Connect using your phone or laptop to select your home Wi-Fi.");
        } else if (arg == "reset" || arg == "clear") {
            Serial.println("Clearing saved Wi-Fi credentials and restarting...");
            WifiService::instance().resetSettings();
        } else {
            // wifi status
            const WifiService& ws = WifiService::instance();
            Serial.println("\033[1;36m=== WI-FI & WEB DASHBOARD STATUS ===\033[0m");
            Serial.printf("State:       %s\n", ws.isConnected() ? "Connected" : (ws.isPortalActive() ? "Hotspot Active (HeltecOS-Setup)" : "Disconnected"));
            if (ws.isConnected()) {
                Serial.printf("SSID:        %s\n", ws.getSSID().c_str());
                Serial.printf("IP Address:  %s\n", ws.getIP().c_str());
                Serial.printf("Web URL:     http://%s or http://heltecos.local\n", ws.getIP().c_str());
                Serial.printf("Signal RSSI: %d dBm\n", ws.getRSSI());
                Serial.printf("Time (NTP):  %s (%s, UTC%+d)\n", ws.getTimeString().c_str(), ws.getDateString().c_str(), ws.getTimezoneOffset());
            } else if (ws.isPortalActive()) {
                Serial.println("Hotspot:     HeltecOS-Setup (No password)");
                Serial.println("Portal IP:   http://192.168.4.1");
            }
        }
    } else if (cmd == "tz") {
        if (arg.length() > 0) {
            int tz = arg.toInt();
            WifiService::instance().setTimezoneOffset(tz);
            Serial.printf("Timezone offset set to UTC%+d\n", tz);
        } else {
            Serial.printf("Current timezone offset: UTC%+d. Usage: tz <hours> (e.g. tz 0, tz -5, tz +2)\n", WifiService::instance().getTimezoneOffset());
        }
    } else if (cmd == "reboot") {
        cmdReboot();
    } else if (cmd == "clear") {
        Serial.print("\033[2J\033[H");
        printBanner();
    } else {
        Serial.print("\033[1;31mUnknown command: \033[0m");
        Serial.println(cmd);
        Serial.println("Type 'help' for command list.");
    }
}

void CliShell::cmdHelp() {
    Serial.println("\033[1;33mAvailable Commands:\033[0m");
    Serial.println("  sysinfo         - Print hardware specs, flash, memory, and runtime");
    Serial.println("  sensor          - Read BME/BMP280 temperature, humidity, pressure");
    Serial.println("  battery         - Show battery voltage, charge status, and level");
    Serial.println("  offset <val>    - Calibrate temp offset in C (e.g. offset -1.5)");
    Serial.println("  i2c scan        - Scan external I2C bus (SDA: GPIO 4, SCL: GPIO 5)");
    Serial.println("  ps              - List FreeRTOS tasks and system memory status");
    Serial.println("  screen <0..4>   - Switch OLED GUI page (0:Dash 1:Sensor 2:WiFi 3:Tasks 4:About)");
    Serial.println("  prg             - Simulate clicking the physical PRG button");
    Serial.println("  led <on|off|t>  - Control onboard user LED (GPIO 35)");
    Serial.println("  wifi            - Show Wi-Fi status, IP, and Web Dashboard URL");
    Serial.println("  wifi scan       - Scan nearby 2.4GHz WiFi networks");
    Serial.println("  wifi portal     - Launch 'HeltecOS-Setup' Captive Portal hotspot");
    Serial.println("  wifi reset      - Clear saved Wi-Fi credentials from Flash");
    Serial.println("  tz <hours>      - Set NTP timezone offset (e.g. tz -5 or tz +2)");
    Serial.println("  clear           - Clear terminal screen");
    Serial.println("  reboot          - Restart the ESP32-S3");
}

void CliShell::cmdSysinfo() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    Serial.println("\033[1;36m=== SYSTEM HARDWARE INFO ===\033[0m");
    Serial.print("Model:        ESP32-S3 (Revision ");
    Serial.print(chip_info.revision);
    Serial.println(")");
    Serial.print("CPU Cores:    ");
    Serial.println(chip_info.cores);
    Serial.print("CPU Clock:    ");
    Serial.print(ESP.getCpuFreqMHz());
    Serial.println(" MHz");
    Serial.print("Flash Size:   ");
    Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
    Serial.println(" MB");
    Serial.print("Total Heap:   ");
    Serial.print(ESP.getHeapSize() / 1024);
    Serial.println(" KB");
    Serial.print("Free Heap:    ");
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(" KB");
    Serial.print("Min Free Heap:");
    Serial.print(ESP.getMinFreeHeap() / 1024);
    Serial.println(" KB");
    Serial.print("MAC Address:  ");
    Serial.println(WiFi.macAddress());
    Serial.print("SDK Version:  ");
    Serial.println(ESP.getSdkVersion());
    Serial.print("Uptime:       ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
}

void CliShell::cmdSensor() {
    const TelemetryData& t = BoardHAL::instance().getTelemetry();
    Serial.println("\033[1;36m=== ENVIRONMENTAL SENSOR TELEMETRY ===\033[0m");
    if (!t.sensor_present) {
        Serial.println("\033[1;31mStatus: SENSOR NOT DETECTED\033[0m");
        Serial.println("Check breadboard connections:");
        Serial.println("  VIN -> 3V3 (Right Row 2)");
        Serial.println("  GND -> GND (Right Row 1)");
        Serial.println("  SCL -> GPIO 5 (Left Row 14)");
        Serial.println("  SDA -> GPIO 4 (Left Row 15)");
        Serial.println("Tip: Run 'i2c scan' to check if the chip responds on the bus.");
        return;
    }

    Serial.print("Chip Type:    ");
    Serial.println(t.is_bme ? "BME280 (Temperature, Humidity, Pressure)" : "BMP280 (Temperature, Pressure)");
    Serial.printf("Temperature:  %.2f °C (%.2f °F) [Raw: %.2f °C, Offset: %.1f °C]\n", 
                  t.temperature_c, (t.temperature_c * 9.0f / 5.0f) + 32.0f,
                  t.raw_temperature_c, t.temp_offset);
    if (t.is_bme) {
        Serial.printf("Humidity:     %.2f %%RH\n", t.humidity_pct);
    }
    Serial.printf("Pressure:     %.2f hPa\n", t.pressure_hpa);
    Serial.printf("Est Altitude: %.1f meters\n", t.altitude_m);
}

void CliShell::cmdBattery() {
    const TelemetryData& t = BoardHAL::instance().getTelemetry();
    Serial.println("\033[1;36m=== POWER & BATTERY TELEMETRY ===\033[0m");
    Serial.printf("Voltage:      %.3f V\n", t.vbat_volts);
    Serial.printf("Est Charge:   %d %%\n", t.battery_pct);
    Serial.printf("Power Source: %s\n", t.is_usb_power ? "USB Port (5V)" : "LiPo Battery (JST)");
    Serial.printf("Live Current: %+.1f mA\n", t.current_ma);
    Serial.printf("Power Draw:   %.1f mW (%.2f W)\n", t.power_mw, t.power_mw / 1000.0f);

    int rem_h = (int)t.time_remaining_hours;
    int rem_m = (int)((t.time_remaining_hours - rem_h) * 60.0f);
    if (t.is_full) {
        Serial.println("Time to Full: Battery 100% Charged");
    } else if (t.is_charging) {
        Serial.printf("Time to Full: ~%d hrs %02d mins\n", rem_h, rem_m);
    } else {
        Serial.printf("Time Left:    ~%d hrs %02d mins remaining\n", rem_h, rem_m);
    }

    const char* state_str = "Discharging / Battery";
    if (t.is_full) {
        state_str = "Fully Charged (100%)";
    } else if (t.is_charging) {
        state_str = "USB Powered / Charging";
    }
    Serial.printf("Power State:  %s\n", state_str);
}

void CliShell::cmdI2cScan() {
    TwoWire& wire = BoardHAL::instance().getSensorWire();
    wire.setTimeOut(10);
    Serial.println("\033[1;36m=== SCANNING EXTERNAL I2C BUS (SDA:4, SCL:5) ===\033[0m");
    int count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        wire.beginTransmission(addr);
        if (wire.endTransmission() == 0) {
            Serial.printf("Found I2C device at 0x%02X", addr);
            if (addr == 0x76 || addr == 0x77) {
                Serial.print(" (Likely BME280 or BMP280)");
            }
            Serial.println();
            count++;
        }
    }
    if (count == 0) {
        Serial.println("No I2C devices found on GPIO 4/5.");
    } else {
        Serial.printf("Total devices found: %d\n", count);
    }
}

void CliShell::cmdPs() {
    Serial.println("\033[1;36m=== FREERTOS TASK MONITOR ===\033[0m");
    Serial.println("Task Name      Core  Prio  StackWaterMark");
    Serial.println("-----------------------------------------");
    Serial.printf("GUI_Task       1     1     %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
    Serial.printf("Free Heap:     %lu KB\n", (unsigned long)(esp_get_free_heap_size() / 1024));
    Serial.printf("Min Free Heap: %lu KB\n", (unsigned long)(esp_get_minimum_free_heap_size() / 1024));
}

void CliShell::cmdOffset(const String& arg) {
    if (arg.length() == 0) {
        Serial.printf("Current temperature offset: %.2f °C\n", BoardHAL::instance().getTempOffset());
        Serial.println("Usage: offset <value> (e.g. offset -1.5 or offset 0.0)");
    } else {
        float val = arg.toFloat();
        BoardHAL::instance().setTempOffset(val);
        Serial.printf("Temperature offset set to: %.2f °C\n", val);
    }
}

void CliShell::cmdScreen(const String& arg) {
    if (arg.length() == 0) {
        GuiManager::instance().nextScreen();
        Serial.printf("Switched to screen %d\n", GuiManager::instance().getScreen());
    } else {
        int s = arg.toInt();
        if (s >= 0 && s < SCREEN_COUNT) {
            GuiManager::instance().setScreen((GuiScreen)s);
            Serial.printf("OLED Screen set to %d\n", s);
        } else {
            Serial.println("Invalid screen index. Use 0..4.");
        }
    }
}

void CliShell::cmdLed(const String& arg) {
    if (arg == "on" || arg == "1") {
        BoardHAL::instance().setLED(true);
        Serial.println("User LED: ON");
    } else if (arg == "off" || arg == "0") {
        BoardHAL::instance().setLED(false);
        Serial.println("User LED: OFF");
    } else if (arg == "toggle" || arg == "t" || arg == "") {
        BoardHAL::instance().toggleLED();
        Serial.printf("User LED: %s\n", BoardHAL::instance().getLEDState() ? "ON" : "OFF");
    } else {
        Serial.println("Usage: led on | off | toggle");
    }
}

void CliShell::cmdWifiScan() {
    Serial.println("\033[1;36m=== SCANNING 2.4GHz WIFI NETWORKS ===\033[0m");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.println("No networks found.");
    } else {
        Serial.printf("Found %d networks:\n", n);
        Serial.println("RSSI (dBm)  Ch  Encryption  SSID");
        Serial.println("----------------------------------------------");
        for (int i = 0; i < n; ++i) {
            Serial.printf("%-11ld %-3ld %-11s %s\n", 
                (long)WiFi.RSSI(i), 
                (long)WiFi.channel(i), 
                (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Encrypted",
                WiFi.SSID(i).c_str());
        }
    }
    WiFi.scanDelete();
}

void CliShell::cmdReboot() {
    Serial.println("\033[1;31mRebooting HeltecOS...\033[0m");
    delay(200);
    esp_restart();
}
