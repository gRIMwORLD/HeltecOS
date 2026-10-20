#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include "board_hal.h"

enum GuiScreen {
    SCREEN_DASHBOARD = 0,
    SCREEN_ENV_SENSOR = 1,
    SCREEN_WIFI = 2,
    SCREEN_TASKS = 3,
    SCREEN_ABOUT = 4,
    SCREEN_COUNT = 5
};

class GuiManager {
public:
    static GuiManager& instance();

    void init();
    void render();
    void nextScreen();
    void setScreen(GuiScreen screen);
    GuiScreen getScreen() const;
    void setPower(bool on);
    void togglePower();
    bool isPowered() const;

private:
    GuiManager();

    void drawStatusBar(const TelemetryData& t);
    void drawDashboard(const TelemetryData& t);
    void drawEnvSensor(const TelemetryData& t);
    void drawWiFiScreen(const TelemetryData& t);
    void drawTasksScreen(const TelemetryData& t);
    void drawAboutScreen(const TelemetryData& t);

    U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2;
    GuiScreen current_screen;
    volatile bool is_powered;
    volatile bool power_requested;
    uint32_t last_frame_time;
};
