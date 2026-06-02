#include "stall_guard.h"
#include "config.h"

StallGuard stallGuard;

volatile bool     g_stallPending = false;
volatile uint32_t g_stallTime    = 0;

static void IRAM_ATTR onDiagRising() {
    if (!g_stallPending) {
        g_stallTime    = millis();
        g_stallPending = true;
    }
}

void StallGuard::init(StallCallback cb) {
    _cb = cb;
    pinMode(PIN_DIAG, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_DIAG), onDiagRising, RISING);
}

void StallGuard::process() {
    if (!g_stallPending) return;
    if (millis() - g_stallTime < STALL_DEBOUNCE_MS) return;

    g_stallPending = false;
    if (_cb) _cb();
}
