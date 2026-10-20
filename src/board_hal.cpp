#include "board_hal.h"

BoardHAL& BoardHAL::instance() {
    static BoardHAL hal;
    return hal;
}

BoardHAL::BoardHAL() : led_state(false), sensor_detected(false), is_bme280(false),
                       temp_offset(-1.0f), last_button_state(true), last_debounce_time(0) {
    memset(&telemetry, 0, sizeof(telemetry));
}

void BoardHAL::init() {
    // 1. Configure Vext power rail
    // Driving PIN_VEXT LOW enables 3.3V rail for OLED and external sensors
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, LOW);
    delay(50);

    // 2. Configure user LED and button
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // 3. Configure OLED reset line
    pinMode(PIN_OLED_RST, OUTPUT);
    digitalWrite(PIN_OLED_RST, LOW);
    delay(20);
    digitalWrite(PIN_OLED_RST, HIGH);
    delay(50);

    // 4. Initialize Battery Monitor
    initBattery();

    // 5. Initialize External I2C and BME/BMP280 Sensor
    initSensor();

    // Initial telemetry sample
    updateTelemetry();
}

void BoardHAL::setVext(bool enable) {
    // Active LOW: LOW = ON, HIGH = OFF
    digitalWrite(PIN_VEXT, enable ? LOW : HIGH);
}

void BoardHAL::setLED(bool on) {
    led_state = on;
    digitalWrite(PIN_LED, on ? HIGH : LOW);
}

void BoardHAL::toggleLED() {
    setLED(!led_state);
}

bool BoardHAL::getLEDState() {
    return led_state;
}

bool BoardHAL::isButtonPressed() {
    return digitalRead(PIN_BUTTON) == LOW;
}

bool BoardHAL::checkButtonEvent() {
    bool current_state = (digitalRead(PIN_BUTTON) == LOW);
    bool clicked = false;
    uint32_t now = millis();

    if (current_state != last_button_state) {
        if (now - last_debounce_time > 50) {
            if (current_state) {
                // Button was just pressed down
                clicked = true;
            }
            last_debounce_time = now;
            last_button_state = current_state;
        }
    }
    return clicked;
}

void BoardHAL::initBattery() {
    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, HIGH); // Default OFF to save power
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

float BoardHAL::readBatteryVoltage() {
    // Enable ADC divider circuit
    digitalWrite(PIN_ADC_CTRL, LOW);
    delayMicroseconds(500);

    // Read average of multiple samples
    uint32_t raw_mv = 0;
    const int SAMPLES = 8;
    for (int i = 0; i < SAMPLES; i++) {
        raw_mv += analogReadMilliVolts(PIN_VBAT_ADC);
        delayMicroseconds(100);
    }
    raw_mv /= SAMPLES;

    // Turn off divider circuit to prevent drain
    digitalWrite(PIN_ADC_CTRL, HIGH);

    // Voltage divider: (390k + 100k) / 100k = 4.90
    // Factor ~4.90. In practice ~4.95 with PCB traces/diode drop
    float volts = (raw_mv * 4.95f) / 1000.0f;
    return volts;
}

void BoardHAL::recoverI2C() {
    extWire.end();
    pinMode(PIN_EXT_SCL, OUTPUT);
    pinMode(PIN_EXT_SDA, INPUT_PULLUP);
    for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_EXT_SCL, LOW);
        delayMicroseconds(5);
        digitalWrite(PIN_EXT_SCL, HIGH);
        delayMicroseconds(5);
    }
    pinMode(PIN_EXT_SCL, INPUT_PULLUP);
    extWire.begin(PIN_EXT_SDA, PIN_EXT_SCL, 100000);
    extWire.setTimeOut(20);
}

void BoardHAL::initSensor() {
    recoverI2C();

    // Quick address check before calling library
    extWire.beginTransmission(0x76);
    byte err76 = extWire.endTransmission();
    extWire.beginTransmission(0x77);
    byte err77 = extWire.endTransmission();

    if (err76 != 0 && err77 != 0) {
        sensor_detected = false;
        return;
    }

    uint8_t addr = (err76 == 0) ? 0x76 : 0x77;

    // 1. Try BME280
    if (bme.begin(addr, &extWire)) {
        sensor_detected = true;
        is_bme280 = true;
        // Configure Bosch Weather Monitoring Low-Power Forced Mode
        bme.setSampling(Adafruit_BME280::MODE_FORCED,
                        Adafruit_BME280::SAMPLING_X1, // Temperature: 1x (no self-heat)
                        Adafruit_BME280::SAMPLING_X1, // Pressure: 1x
                        Adafruit_BME280::SAMPLING_X1, // Humidity: 1x
                        Adafruit_BME280::FILTER_OFF);
        return;
    }

    // 2. If not BME280, try BMP280
    if (bmp.begin(addr)) {
        sensor_detected = true;
        is_bme280 = false;
        bmp.setSampling(Adafruit_BMP280::MODE_FORCED,
                        Adafruit_BMP280::SAMPLING_X1,
                        Adafruit_BMP280::SAMPLING_X1,
                        Adafruit_BMP280::FILTER_OFF);
        return;
    }

    sensor_detected = false;
}

TwoWire& BoardHAL::getSensorWire() {
    return extWire;
}

void BoardHAL::updateTelemetry() {
    // 1. Battery status
    float v = readBatteryVoltage();
    telemetry.vbat_volts = v;

    // Approximate LiPo percentage
    if (v >= 4.20f) {
        telemetry.battery_pct = 100;
    } else if (v <= 3.25f) {
        telemetry.battery_pct = 0;
    } else {
        telemetry.battery_pct = (int)(((v - 3.25f) / (4.20f - 3.25f)) * 100.0f);
    }
    telemetry.is_charging = (v > 4.30f); // USB VBUS boost when plugged in

    // 2. Environmental sensor status
    telemetry.sensor_present = sensor_detected;
    telemetry.is_bme = is_bme280;

    if (sensor_detected) {
        float t = NAN, h = 0.0f, p = NAN, a = 0.0f;
        if (is_bme280) {
            bme.takeForcedMeasurement(); // Single 9ms pulse, sleeps between samples
            t = bme.readTemperature();
            h = bme.readHumidity();
            p = bme.readPressure() / 100.0F;
            a = bme.readAltitude(1013.25F);
        } else {
            bmp.takeForcedMeasurement();
            t = bmp.readTemperature();
            h = 0.0f;
            p = bmp.readPressure() / 100.0F;
            a = bmp.readAltitude(1013.25F);
        }

        if (!isnan(t) && !isnan(p) && p > 300.0f && p < 1200.0f) {
            telemetry.raw_temperature_c = t;
            telemetry.temp_offset = temp_offset;
            telemetry.temperature_c = t + temp_offset;
            telemetry.humidity_pct = h;
            telemetry.pressure_hpa = p;
            telemetry.altitude_m = a;
            telemetry.sensor_sample_count++;
        } else {
            // Read failed - bus error or unplugged
            sensor_detected = false;
            recoverI2C();
        }
    } else {
        // Retry probe periodically
        static uint32_t last_retry = 0;
        if (millis() - last_retry > 2000) {
            last_retry = millis();
            initSensor();
        }
    }

    // 3. System stats
    telemetry.free_heap = esp_get_free_heap_size();
    telemetry.min_free_heap = esp_get_minimum_free_heap_size();
    telemetry.uptime_sec = millis() / 1000;
}

TelemetryData& BoardHAL::getTelemetry() {
    return telemetry;
}

void BoardHAL::setTempOffset(float offset) {
    temp_offset = offset;
    telemetry.temp_offset = offset;
    telemetry.temperature_c = telemetry.raw_temperature_c + offset;
}

float BoardHAL::getTempOffset() const {
    return temp_offset;
}
