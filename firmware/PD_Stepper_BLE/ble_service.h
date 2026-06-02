#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

class BleService {
public:
    void init();
    void processNotifications();  // call every loop iteration
    void postEvent(const char* json);  // thread-safe event enqueue
    bool    isConnected()    const { return _connectionCount > 0; }
    uint8_t connectionCount() const { return _connectionCount; }
    void clientConnected();
    void clientDisconnected();

private:
    uint8_t  _connectionCount = 0;
    uint32_t _lastStatusMs    = 0;

    NimBLECharacteristic* _pStatusChar  = nullptr;
    NimBLECharacteristic* _pConfigChar  = nullptr;

    void sendStatus();
    void buildConfigJson(char* buf, size_t len);
};

extern BleService bleService;
