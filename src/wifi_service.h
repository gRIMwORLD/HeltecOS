#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>

enum WifiState {
    WIFI_DISCONNECTED,
    WIFI_PORTAL_ACTIVE,
    WIFI_CONNECTING,
    WIFI_CONNECTED
};

class WifiService {
public:
    static WifiService& instance();

    void init();
    void process();

    // Portal control
    void startPortal();
    void resetSettings();

    // Status queries
    WifiState getState() const;
    bool isConnected() const;
    bool isPortalActive() const;
    String getSSID() const;
    String getIP() const;
    int getRSSI() const;

    // Real-Time Clock / NTP
    bool isTimeSynced() const;
    String getTimeString() const;
    String getDateString() const;
    void setTimezoneOffset(int hours);
    int getTimezoneOffset() const;

private:
    WifiService();
    void setupWebServer();
    void syncNTP();

    // Web handlers
    void handleRoot();
    void handleApiTelemetry();
    void handleApiAction();

    WiFiManager wm;
    WebServer server;
    WifiState state;
    bool time_synced;
    int tz_offset_hours;
    uint32_t last_ntp_check;
    uint32_t last_wifi_status_check;
};
