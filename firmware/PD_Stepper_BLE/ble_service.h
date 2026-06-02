#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

class BleService {
public:
    void init();
    void processNotifications();  // call every loop iteration
    void postEvent(const char* json);  // thread-safe event enqueue
    bool isConnected() const { return _connected; }
    void setConnected(bool val) { _connected = val; }

private:
    bool     _connected = false;
    uint32_t _lastStatusMs = 0;

    NimBLECharacteristic* _pStatusChar  = nullptr;
    NimBLECharacteristic* _pConfigChar  = nullptr;

    void sendStatus();
    void buildConfigJson(char* buf, size_t len);
};

extern BleService bleService;
