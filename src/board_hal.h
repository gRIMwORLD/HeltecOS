#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>

// Board Pin Definitions (Heltec WiFi Kit 32 V3)
#define PIN_VEXT        36
#define PIN_LED         35
#define PIN_BUTTON      0

#define PIN_OLED_RST    21
#define PIN_OLED_SCL    18
#define PIN_OLED_SDA    17

// Battery ADC pins
#define PIN_VBAT_ADC    1
#define PIN_ADC_CTRL    37

// External Sensor I2C (BME280 / BMP280) on Left Header
#define PIN_EXT_SDA     4   // Breadboard Row 15
#define PIN_EXT_SCL     5   // Breadboard Row 14

struct TelemetryData {
    // Battery & Power
    float vbat_volts;
    int battery_pct;
    bool is_charging;
    bool is_full;

    // Environmental Sensor
    bool sensor_present;
    bool is_bme; // true = BME280 (with humidity), false = BMP280
    uint32_t sensor_sample_count;
    float raw_temperature_c;
    float temperature_c; // adjusted with offset
    float temp_offset;
    float humidity_pct;
    float pressure_hpa;
    float altitude_m;

    // System
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint32_t uptime_sec;
};

class BoardHAL {
public:
    static BoardHAL& instance();

    void init();
    void updateTelemetry();
    TelemetryData& getTelemetry();
    void recoverI2C();
    void setTempOffset(float offset);
    float getTempOffset() const;

    // Power controls
    void setVext(bool enable);
    void setLED(bool on);
    void toggleLED();
    bool getLEDState();

    // User input
    bool isButtonPressed();
    bool checkButtonEvent(); // detects single click with debounce

    // I2C wire for external sensor
    TwoWire& getSensorWire();

private:
    BoardHAL();
    void initBattery();
    void initSensor();
    float readBatteryVoltage();

    TelemetryData telemetry;
    bool led_state;
    bool sensor_detected;
    bool is_bme280;
    float temp_offset;

    // Battery telemetry filter
    float vbat_baseline;
    float vbat_samples[8];
    uint8_t vbat_sample_idx;

    TwoWire extWire = TwoWire(1);
    Adafruit_BME280 bme;
    Adafruit_BMP280 bmp = Adafruit_BMP280(&extWire);

    // Button debounce state
    bool last_button_state;
    uint32_t last_debounce_time;
};
