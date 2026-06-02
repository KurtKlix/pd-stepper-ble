#include "ble_service.h"
#include "config.h"
#include "encoder.h"
#include "motor_control.h"
#include "endstop.h"
#include <ArduinoJson.h>
#include <string.h>

BleService bleService;

// ── Event queue (ISR-safe ring buffer) ───────────────────────────────────────

struct EventQueue {
    char   items[EVENT_QUEUE_SIZE][256];
    uint8_t head = 0;
    uint8_t tail = 0;

    bool empty() const { return head == tail; }
    bool push(const char* s) {
        uint8_t next = (head + 1) % EVENT_QUEUE_SIZE;
        if (next == tail) return false; // full
        strncpy(items[head], s, 255);
        items[head][255] = '\0';
        head = next;
        return true;
    }
    const char* pop() {
        if (empty()) return nullptr;
        const char* s = items[tail];
        tail = (tail + 1) % EVENT_QUEUE_SIZE;
        return s;
    }
};

static EventQueue eventQueue;

// ── BLE callbacks ─────────────────────────────────────────────────────────────

class CommandCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
        NimBLEAttValue attVal = pChar->getValue();
        if (attVal.length() == 0) return;
        std::string val(attVal.c_str(), attVal.length());

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, val.c_str());

        char ack[128];
        if (err) {
            snprintf(ack, sizeof(ack),
                "{\"type\":\"error\",\"msg\":\"JSON parse failed\",\"ts\":%lu}", millis());
        } else {
            const char* cmd = doc["cmd"] | "unknown";
            motorControl.applyCommand(doc);
            snprintf(ack, sizeof(ack),
                "{\"type\":\"ack\",\"cmd\":\"%s\",\"ok\":true,\"ts\":%lu}", cmd, millis());
        }
        bleService.postEvent(ack);
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        bleService.clientConnected();
        Serial.printf("[BLE] client connected (%d total)\n", bleService.connectionCount());
        // Keep advertising so additional clients (phone, API) can also connect
        NimBLEDevice::getAdvertising()->start();
    }
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        bleService.clientDisconnected();
        Serial.printf("[BLE] client disconnected (%d remaining)\n", bleService.connectionCount());
        if (bleService.connectionCount() == 0) {
            // Only stop the motor when ALL clients are gone
            JsonDocument doc;
            doc["cmd"] = "stop";
            motorControl.applyCommand(doc);
            Serial.println("[BLE] all clients disconnected — motor stopped");
        }
        // Always restart advertising so the disconnected client can reconnect
        NimBLEDevice::getAdvertising()->start();
    }
};

// ── Init ──────────────────────────────────────────────────────────────────────

static float readVbusVolts() {
    // Average 10 ADC samples to reduce noise
    uint32_t sum = 0;
    for (int i = 0; i < 10; i++) sum += analogReadMilliVolts(PIN_VBUS);
    float mv = (float)(sum / 10);
    return (mv / 1000.0f) / VBUS_DIV_RATIO;
}

void BleService::init() {
    analogSetPinAttenuation(PIN_VBUS, ADC_11db); // full 3.3 V range
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(9); // +9 dBm tx power

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(BLE_SERVICE_UUID);

    // Command characteristic (write)
    NimBLECharacteristic* pCmdChar = pService->createCharacteristic(
        BLE_CMD_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pCmdChar->setCallbacks(new CommandCallback());

    // Status characteristic (notify)
    _pStatusChar = pService->createCharacteristic(
        BLE_STATUS_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Config characteristic (read)
    _pConfigChar = pService->createCharacteristic(
        BLE_CONFIG_UUID,
        NIMBLE_PROPERTY::READ
    );
    char cfgBuf[256];
    buildConfigJson(cfgBuf, sizeof(cfgBuf));
    _pConfigChar->setValue(cfgBuf);

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->setName(BLE_DEVICE_NAME);
    pAdv->addServiceUUID(BLE_SERVICE_UUID);
    pAdv->enableScanResponse(true);
    pAdv->start();
    Serial.println("[BLE] advertising as: " BLE_DEVICE_NAME);
}

// ── Main loop work ────────────────────────────────────────────────────────────

void BleService::processNotifications() {
    if (_connectionCount == 0) return;

    // Drain event queue first (events take priority)
    bool sentEvent = false;
    while (!eventQueue.empty()) {
        const char* ev = eventQueue.pop();
        if (ev) {
            _pStatusChar->setValue(ev);
            _pStatusChar->notify();
            sentEvent = true;
        }
    }

    // Periodic status notification (reset timer if an event was just sent)
    if (sentEvent) {
        _lastStatusMs = millis();
        return;
    }

    if (millis() - _lastStatusMs >= BLE_STATUS_INTERVAL_MS) {
        _lastStatusMs = millis();
        sendStatus();

        // Refresh config characteristic in case settings changed
        char cfgBuf[256];
        buildConfigJson(cfgBuf, sizeof(cfgBuf));
        _pConfigChar->setValue(cfgBuf);
    }
}

void BleService::clientConnected()    { _connectionCount++; }
void BleService::clientDisconnected() { if (_connectionCount > 0) _connectionCount--; }

void BleService::postEvent(const char* json) {
    eventQueue.push(json);
}

void BleService::sendStatus() {
    float pos  = encoder.getAbsoluteDeg();
    float vel  = encoder.getVelocityDps();
    float tgt  = motorControl.getTargetDeg();
    MotorMode mode = motorControl.getMode();

    const char* modeStr = "idle";
    if      (mode == MotorMode::MOVING)   modeStr = "position";
    else if (mode == MotorMode::VELOCITY) modeStr = "velocity";
    else if (mode == MotorMode::HOMING)   modeStr = "homing";

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"status\",\"pos_deg\":%.2f,\"vel_dps\":%.2f,"
        "\"target_deg\":%.2f,\"enabled\":%s,\"mode\":\"%s\","
        "\"current_ma\":%d,\"microsteps\":%d,\"voltage_v\":%.2f,"
        "\"clients\":%d,\"ts\":%lu}",
        pos, vel, tgt,
        motorControl.getConfig().enabled ? "true" : "false",
        modeStr,
        motorControl.getConfig().currentMa,
        motorControl.getConfig().microsteps,
        readVbusVolts(),
        _connectionCount,
        millis()
    );

    _pStatusChar->setValue(buf);
    _pStatusChar->notify();
}

void BleService::buildConfigJson(char* buf, size_t len) {
    MotorConfig& cfg = motorControl.getConfig();
    snprintf(buf, len,
        "{\"fw_ver\":\"%s\",\"device\":\"%s\","
        "\"current_ma\":%d,\"microsteps\":%d,\"speed_sps\":%d,"
        "\"closed_loop\":%d,\"mapping_dir\":%d,"
        "\"endstop_gpio\":%d,\"stallguard_threshold\":50,"
        "\"voltage_v\":%.2f,\"pd_target_v\":%d,"
        "\"endstop_mode\":\"%s\"}",
        FW_VERSION, BLE_DEVICE_NAME,
        cfg.currentMa, cfg.microsteps, cfg.speedSps,
        cfg.closedLoopType, cfg.mappingDir,
        PIN_ENDSTOP,
        readVbusVolts(), PD_VOLTAGE,
        (endstop.getMode() == EndstopMode::NO) ? "NO" : "NC"
    );
}
