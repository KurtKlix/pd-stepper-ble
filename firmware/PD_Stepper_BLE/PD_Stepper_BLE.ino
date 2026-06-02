/*
 * PD-Stepper BLE Firmware
 *
 * Controls the PD-Stepper motor controller via Bluetooth Low Energy.
 * Exposes a JSON command/notify interface over custom GATT characteristics.
 *
 * Required libraries (install via Arduino Library Manager):
 *   - NimBLE-Arduino
 *   - ArduinoJson  (v7)
 *   - TMCStepper
 */

#include "config.h"
#include "encoder.h"
#include "motor_control.h"
#include "stall_guard.h"
#include "endstop.h"
#include "ble_service.h"

// ── Event callbacks (posted from ISR debounce, forwarded to BLE) ─────────────

static void onStall() {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"stall\",\"pos_deg\":%.2f,\"ts\":%lu}",
        encoder.getAbsoluteDeg(), millis());
    bleService.postEvent(buf);

    // Stop motor immediately on stall
    JsonDocument doc;
    doc["cmd"] = "stop";
    motorControl.applyCommand(doc);
}

static void onEndstop() {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"endstop\",\"pos_deg\":%.2f,\"ts\":%lu}",
        encoder.getAbsoluteDeg(), millis());
    bleService.postEvent(buf);

    if (motorControl.getMode() == MotorMode::HOMING) {
        // Zero encoder and stop
        JsonDocument doc;
        doc["cmd"] = "stop";
        motorControl.applyCommand(doc);
        encoder.zero();

        char homeBuf[96];
        snprintf(homeBuf, sizeof(homeBuf),
            "{\"type\":\"home_complete\",\"pos_deg\":0.0,\"ts\":%lu}", millis());
        bleService.postEvent(homeBuf);
    }
}

// ── Motor event callback (position_reached generated inside motorControl) ─────

static void onMotorEvent(const char* json) {
    bleService.postEvent(json);
}

// ── Arduino setup / loop ─────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    // Wait up to 3 s for the USB CDC Serial Monitor to connect after reset.
    // Native USB on ESP32-S3 re-enumerates on every reset, so messages sent
    // immediately at boot are lost before the host reconnects.
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);
    Serial.println("[PD-Stepper BLE] starting...");

    encoder.init();
    motorControl.init(onMotorEvent);
    stallGuard.init(onStall);
    endstop.init(onEndstop);
    bleService.init();

    Serial.println("[PD-Stepper BLE] ready");
}

void loop() {
    encoder.update();
    motorControl.controlLoop();
    stallGuard.process();
    endstop.process();
    bleService.processNotifications();
}
