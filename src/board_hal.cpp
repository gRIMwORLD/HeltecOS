#include "board_hal.h"
#include "driver/gpio.h"
#include <WiFi.h>

BoardHAL& BoardHAL::instance() {
    static BoardHAL hal;
    return hal;
}

BoardHAL::BoardHAL() : led_state(false), oled_powered(true), sensor_detected(false), is_bme280(false),
                       temp_offset(-1.0f), vbat_filtered(0.0f), smoothed_pct(-1),
                       last_charging_state(false), prev_v_raw(0.0f), usb_power_latched(true),
                       vbat_hist_count(0), vbat_baseline(0.0f), vbat_sample_idx(0),
                       last_button_state(true), last_debounce_time(0) {
    memset(&telemetry, 0, sizeof(telemetry));
    telemetry.oled_powered = true;
    memset(vbat_samples, 0, sizeof(vbat_samples));
    memset(vbat_history, 0, sizeof(vbat_history));
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

void BoardHAL::setOledPowered(bool on) {
    oled_powered = on;
    telemetry.oled_powered = on;
}

bool BoardHAL::isOledPowered() const {
    return oled_powered;
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

bool BoardHAL::isUsbConnected() {
    return usb_power_latched;
}

float BoardHAL::readBatteryVoltage() {
    // Enable ADC divider circuit
    digitalWrite(PIN_ADC_CTRL, LOW);
    delayMicroseconds(500);

    // Read average of multiple samples
    uint32_t raw_mv = 0;
    const int SAMPLES = 16;
    for (int i = 0; i < SAMPLES; i++) {
        raw_mv += analogReadMilliVolts(PIN_VBAT_ADC);
        delayMicroseconds(50);
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

int BoardHAL::calculateLipoPct(float volts, bool is_charging) {
    float v = volts;
    if (is_charging) {
        // TP4054 500mA charge current + board load switch causes a ~150-170mV total loop IR drop
        // (cell + PCM + JST connector + wires).
        // In CC phase (< 4.10V), offset is ~0.15V.
        // In CV phase (>= 4.10V), charge current tapers down towards 0 at 4.20V.
        float offset = 0.15f;
        if (v >= 4.10f) {
            offset = 0.15f * (4.20f - v) / (4.20f - 4.10f);
            if (offset < 0.0f) offset = 0.0f;
        }
        v -= offset;
    }

    // 11-point Piecewise Linear Interpolation for 3.7V JXP 804070 LiPo
    struct LipoPoint {
        float volts;
        int pct;
    };

    static const LipoPoint curve[] = {
        { 4.18f, 100 },
        { 4.10f,  92 },
        { 4.00f,  83 },
        { 3.90f,  72 },
        { 3.83f,  60 },
        { 3.78f,  50 },
        { 3.74f,  40 },
        { 3.70f,  28 },
        { 3.65f,  18 },
        { 3.58f,   9 },
        { 3.45f,   3 },
        { 3.30f,   0 }
    };
    const int N = sizeof(curve) / sizeof(curve[0]);

    if (v >= curve[0].volts) return 100;
    if (v <= curve[N - 1].volts) return 0;

    for (int i = 0; i < N - 1; i++) {
        if (v >= curve[i + 1].volts) {
            float v_high = curve[i].volts;
            float v_low  = curve[i + 1].volts;
            int   p_high = curve[i].pct;
            int   p_low  = curve[i + 1].pct;

            float ratio = (v - v_low) / (v_high - v_low);
            int pct = (int)(p_low + ratio * (p_high - p_low) + 0.5f);
            if (pct > 100) pct = 100;
            if (pct < 0) pct = 0;
            return pct;
        }
    }
    return 0;
}

void BoardHAL::updateTelemetry() {
    // 1. Battery status & physical voltage-dynamics detection
    float v_raw = readBatteryVoltage();

    if (prev_v_raw > 2.0f) {
        // Track recent samples in circular buffer (last 10 samples)
        vbat_history[vbat_hist_count % 10] = v_raw;
        vbat_hist_count++;

        // 1-second and 2-second step detection:
        // Physical current reversal (+460mA <-> -100mA) across ~0.25 ohm loop IR
        // creates an instantaneous step of ~100mV - 170mV.
        // A step of >= 45mV in 1-2 seconds cannot occur under steady load or rest.
        float dv_1s = v_raw - prev_v_raw;
        float dv_2s = dv_1s;
        if (vbat_hist_count >= 3) {
            uint16_t prev2_idx = (vbat_hist_count - 3) % 10;
            dv_2s = v_raw - vbat_history[prev2_idx];
        }

        if (dv_1s <= -0.045f || dv_2s <= -0.045f) {
            usb_power_latched = false;
        } else if (dv_1s >= +0.045f || dv_2s >= +0.045f) {
            usb_power_latched = true;
        }
    } else {
        // First boot initialization
        // A LiPo under 50-100mA load resting voltage never exceeds 4.12V.
        if (v_raw >= 4.15f) {
            usb_power_latched = true;
        } else {
            usb_power_latched = true; // Default to true on boot when connected
        }
        vbat_history[0] = v_raw;
        vbat_hist_count = 1;
    }

    // Absolute physical boundary rule:
    // A LiPo under 100mA discharge load cannot sustain >= 4.17V.
    if (v_raw >= 4.17f) {
        usb_power_latched = true;
    }

    prev_v_raw = v_raw;

    bool usb_pwr = usb_power_latched;
    bool charging_now = usb_pwr && (v_raw < 4.18f);

    // Instant snap on USB plug/unplug transition so the readout doesn't slowly slide
    if (smoothed_pct < 0 || last_charging_state != charging_now) {
        vbat_filtered = v_raw;
    } else {
        // Steady state: EMA low-pass filter (tau ~8-10s) to absorb Wi-Fi radio pulses
        vbat_filtered = (0.12f * v_raw) + (0.88f * vbat_filtered);
    }
    telemetry.vbat_volts = vbat_filtered;
    telemetry.is_usb_power = usb_pwr;
    telemetry.is_full = usb_pwr && (vbat_filtered >= 4.18f);
    telemetry.is_charging = usb_pwr && !telemetry.is_full;

    vbat_samples[vbat_sample_idx % 8] = v_raw;
    vbat_sample_idx++;

    if (vbat_baseline == 0.0f) {
        vbat_baseline = vbat_filtered;
    }

    // Compute target capacity percentage using the JXP 804070 LiPo curve
    int target_pct = calculateLipoPct(vbat_filtered, telemetry.is_charging);

    // Directional Hysteresis / Monotonic clamping:
    // When charging: percentage never dips down from transient load pulses
    // When on battery: percentage never jumps up from transient recovery
    if (smoothed_pct < 0 || last_charging_state != charging_now) {
        smoothed_pct = target_pct;
        last_charging_state = charging_now;
    } else if (telemetry.is_charging) {
        if (target_pct > smoothed_pct) {
            smoothed_pct = target_pct;
        }
    } else {
        if (target_pct < smoothed_pct) {
            smoothed_pct = target_pct;
        }
    }

    if (telemetry.is_full) {
        smoothed_pct = 100;
    }

    telemetry.battery_pct = smoothed_pct;

    // 2. Dynamic Power & Battery Life Estimation (JXP 804070 2500mAh)
    const float BATTERY_CAPACITY_MAH = 2500.0f;

    if (telemetry.is_charging) {
        // Charging phase:
        // In CC phase (< 4.10V), TP4054 supplies ~460mA constant current.
        // In CV phase (>= 4.10V), current tapers down towards 40mA at 4.20V.
        float chg_ma = 460.0f;
        if (vbat_filtered >= 4.10f) {
            chg_ma = 40.0f + 420.0f * ((4.20f - vbat_filtered) / (4.20f - 4.10f));
            if (chg_ma < 40.0f) chg_ma = 40.0f;
            if (chg_ma > 460.0f) chg_ma = 460.0f;
        }
        telemetry.current_ma = chg_ma; // Positive = charging
        telemetry.power_mw = vbat_filtered * chg_ma;

        float remaining_to_fill_mah = BATTERY_CAPACITY_MAH * (1.0f - (smoothed_pct / 100.0f));
        telemetry.time_remaining_hours = (chg_ma > 0.0f) ? (remaining_to_fill_mah / chg_ma) : 0.0f;
    } else {
        // Discharging on battery:
        // Base: ESP32-S3 240MHz dual core (~48mA) + BME280 (<0.1mA) + LDO (~4mA) = ~52mA.
        // Add OLED (~16mA) if powered on:
        float dischg_ma = 52.0f;
        if (oled_powered) {
            dischg_ma += 16.0f;
        }
        // Add User LED (~1.4mA) if on:
        if (led_state) {
            dischg_ma += 1.4f;
        }
        // Add Wi-Fi (~35mA) if active:
        if (WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF) {
            dischg_ma += 35.0f;
        }
        telemetry.current_ma = -dischg_ma; // Negative = discharging
        telemetry.power_mw = vbat_filtered * dischg_ma;

        float remaining_mah = BATTERY_CAPACITY_MAH * (smoothed_pct / 100.0f);
        telemetry.time_remaining_hours = (dischg_ma > 0.0f) ? (remaining_mah / dischg_ma) : 0.0f;
    }

    // 3. Environmental sensor status
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
