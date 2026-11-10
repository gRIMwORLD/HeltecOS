#include "ble_service.h"
#include "gui_manager.h"

BluetoothManager& BluetoothManager::instance() {
    static BluetoothManager mgr;
    return mgr;
}

BluetoothManager::BluetoothManager()
    : pServer(nullptr), pService(nullptr), pTelemetryChar(nullptr),
      pCommandChar(nullptr), pAdvertising(nullptr), connected_count(0), last_update_ms(0) {
}

void BluetoothManager::init() {
    // 1. Initialize BLE Device with unique hardware-derived name
    String devName = BoardHAL::instance().getDeviceName();
    BLEDevice::init(devName);

    // Maximize Bluetooth RF Transmission Power to +9dBm (ESP_PWR_LVL_P9)
    BLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_DEFAULT);
    BLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_ADV);
    BLEDevice::setPower(ESP_PWR_LVL_P9, ESP_BLE_PWR_TYPE_CONN_HDL0);

    // 2. Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(this);

    // 3. Create Custom HeltecOS Telemetry & Control Service
    pService = pServer->createService(HELTECOS_SERVICE_UUID);

    // 4. Create Telemetry Characteristic (Read & Notify)
    pTelemetryChar = pService->createCharacteristic(
        TELEMETRY_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    BLE2902* pDesc = new BLE2902();
    pDesc->setNotifications(true);
    pTelemetryChar->addDescriptor(pDesc);

    // 5. Create Command Characteristic (Write - e.g. toggle OLED, reboot, LED)
    pCommandChar = pService->createCharacteristic(
        COMMAND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCommandChar->setCallbacks(this);

    // Start service
    pService->start();

    // 6. Set up Advertising with standard connectable flags
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(HELTECOS_SERVICE_UUID);
    pAdvertising->setScanResponse(true);

    BLEAdvertisementData advData;
    advData.setName(devName);
    advData.setFlags(0x06); // General Discoverable + BR/EDR Not Supported (BLE Only)
    advData.setCompleteServices(BLEUUID(HELTECOS_SERVICE_UUID));
    pAdvertising->setAdvertisementData(advData);

    BLEDevice::startAdvertising();
    Serial.printf("[BLE] Bluetooth Low Energy service advertising as '%s'\n", devName.c_str());
}

void BluetoothManager::onConnect(BLEServer* pServer) {
    connected_count++;
    Serial.printf("[BLE] Client connected! Active connections: %u\n", connected_count);
    // Restart advertising so additional devices (multi-PC) can also connect
    BLEDevice::startAdvertising();
}

void BluetoothManager::onDisconnect(BLEServer* pServer) {
    if (connected_count > 0) {
        connected_count--;
    }
    Serial.printf("[BLE] Client disconnected. Active connections: %u\n", connected_count);
    // Resume advertising
    BLEDevice::startAdvertising();
}

void BluetoothManager::onWrite(BLECharacteristic* pCharacteristic) {
    String rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
        rxValue.trim();
        Serial.printf("[BLE] Command received: %s\n", rxValue.c_str());

        if (rxValue.equalsIgnoreCase("oled_toggle") || rxValue.equalsIgnoreCase("oled")) {
            GuiManager::instance().togglePower();
        } else if (rxValue.equalsIgnoreCase("oled_on")) {
            GuiManager::instance().setPower(true);
        } else if (rxValue.equalsIgnoreCase("oled_off")) {
            GuiManager::instance().setPower(false);
        } else if (rxValue.equalsIgnoreCase("led_toggle") || rxValue.equalsIgnoreCase("led")) {
            BoardHAL::instance().toggleLED();
        } else if (rxValue.equalsIgnoreCase("screen_next")) {
            GuiManager::instance().nextScreen();
        }
    }
}

void BluetoothManager::updateTelemetry(const TelemetryData& t) {
    // Pack binary payload (24 bytes) for high performance & minimal battery usage:
    // [0-3]   float temp_c
    // [4-7]   float humidity_pct
    // [8-11]  float pressure_hpa
    // [12-15] float vbat_volts
    // [16-17] int16_t current_ma
    // [18]    uint8_t battery_pct
    // [19]    uint8_t flags (bit0: charging, bit1: full, bit2: usb, bit3: oled_on, bit4: led_on, bit5: sensor_present)
    // [20-23] uint32_t uptime_sec

    #pragma pack(push, 1)
    struct BleTelemetryPayload {
        float temp_c;
        float humidity;
        float pressure;
        float vbat;
        int16_t current_ma;
        uint8_t battery_pct;
        uint8_t flags;
        uint32_t uptime;
    } payload;
    #pragma pack(pop)

    payload.temp_c = t.temperature_c;
    payload.humidity = t.humidity_pct;
    payload.pressure = t.pressure_hpa;
    payload.vbat = t.vbat_volts;
    payload.current_ma = (int16_t)t.current_ma;
    payload.battery_pct = (uint8_t)t.battery_pct;
    
    payload.flags = 0;
    if (t.is_charging)      payload.flags |= (1 << 0);
    if (t.is_full)          payload.flags |= (1 << 1);
    if (t.is_usb_power)     payload.flags |= (1 << 2);
    if (t.oled_powered)     payload.flags |= (1 << 3);
    if (BoardHAL::instance().getLEDState()) payload.flags |= (1 << 4);
    if (t.sensor_present)   payload.flags |= (1 << 5);

    payload.uptime = t.uptime_sec;

    // 1. Update GATT Characteristic value & notify connected clients
    if (pTelemetryChar != nullptr) {
        pTelemetryChar->setValue((uint8_t*)&payload, sizeof(payload));
        if (connected_count > 0) {
            pTelemetryChar->notify();
        }
    }
}

bool BluetoothManager::isConnected() const {
    return connected_count > 0;
}

uint32_t BluetoothManager::getConnectedCount() const {
    return connected_count;
}
