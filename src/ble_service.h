#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "board_hal.h"

// Custom 128-bit UUIDs for HeltecOS Environmental & Power Telemetry
#define HELTECOS_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TELEMETRY_CHAR_UUID          "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define COMMAND_CHAR_UUID            "beb5483f-36e1-4688-b7f5-ea07361b26a8"

class BluetoothManager : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
    static BluetoothManager& instance();

    void init();
    void updateTelemetry(const TelemetryData& telemetry);
    bool isConnected() const;
    uint32_t getConnectedCount() const;

    // Server callbacks
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

    // Characteristic callbacks
    void onWrite(BLECharacteristic* pCharacteristic) override;

private:
    BluetoothManager();

    BLEServer* pServer;
    BLEService* pService;
    BLECharacteristic* pTelemetryChar;
    BLECharacteristic* pCommandChar;
    BLEAdvertising* pAdvertising;

    uint32_t connected_count;
    uint32_t last_update_ms;
};
