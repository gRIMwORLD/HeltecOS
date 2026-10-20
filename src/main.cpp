#include <Arduino.h>
#include "board_hal.h"
#include "gui_manager.h"
#include "cli_shell.h"
#include "wifi_service.h"

// FreeRTOS Task Handles
TaskHandle_t guiTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;

// Task: Refresh GUI and handle button events (Core 1)
void taskGUI(void* parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 10 FPS smooth OLED refresh

    for (;;) {
        // Check hardware button (PRG on GPIO 0)
        if (BoardHAL::instance().checkButtonEvent()) {
            GuiManager::instance().nextScreen();
        }

        // Render current screen
        GuiManager::instance().render();

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Task: Periodic sensor and battery sampling (Core 1)
void taskSensor(void* parameter) {
    for (;;) {
        BoardHAL::instance().updateTelemetry();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Sample every 1s
    }
}

void setup() {
    // 1. Initialize Serial Shell immediately for boot diagnostic output
    CliShell::instance().init();
    Serial.println("[BOOT] Initializing Hardware Abstraction Layer...");

    // 2. Initialize Board HAL (Vext power, battery ADC, external I2C)
    BoardHAL::instance().init();
    Serial.println("[BOOT] Board HAL initialized.");

    // 3. Initialize OLED Display and GUI Manager
    Serial.println("[BOOT] Initializing OLED Display...");
    GuiManager::instance().init();
    Serial.println("[BOOT] OLED Display initialized.");

    // 4. Initialize WiFi Service & Captive Portal
    Serial.println("[BOOT] Initializing WiFi Service...");
    WifiService::instance().init();

    // 5. Create FreeRTOS Tasks
    Serial.println("[BOOT] Spawning FreeRTOS tasks...");
    xTaskCreatePinnedToCore(
        taskGUI,
        "GUI_Task",
        4096,
        NULL,
        1,
        &guiTaskHandle,
        1 // Core 1
    );

    xTaskCreatePinnedToCore(
        taskSensor,
        "Sensor_Task",
        4096,
        NULL,
        1,
        &sensorTaskHandle,
        1 // Core 1
    );

    Serial.println("[BOOT] HeltecOS Ready!\n");
}

void loop() {
    // Process WiFiManager & Web Server
    WifiService::instance().process();

    // Core 0 loop processes CLI commands non-blocking
    CliShell::instance().process();
    vTaskDelay(pdMS_TO_TICKS(5));
}
