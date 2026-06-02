#include "endstop.h"
#include "config.h"

Endstop endstop;

volatile bool     g_endstopPending = false;
volatile uint32_t g_endstopTime    = 0;

static void IRAM_ATTR onEndstopISR() {
    if (!g_endstopPending) {
        g_endstopTime    = millis();
        g_endstopPending = true;
    }
}

void Endstop::init(EndstopCallback cb) {
    _cb = cb;
    pinMode(PIN_ENDSTOP, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENDSTOP), onEndstopISR, FALLING);
    _mode = EndstopMode::NO;
}

void Endstop::setMode(EndstopMode mode) {
    _mode = mode;
    detachInterrupt(digitalPinToInterrupt(PIN_ENDSTOP));
    g_endstopPending = false;
    // NO: active-low (switch shorts to GND) → trigger on FALLING
    // NC: active-high (switch opens pull-up) → trigger on RISING
    attachInterrupt(digitalPinToInterrupt(PIN_ENDSTOP), onEndstopISR,
                    (mode == EndstopMode::NO) ? FALLING : RISING);
}

void Endstop::process() {
    if (!g_endstopPending) return;
    if (millis() - g_endstopTime < ENDSTOP_DEBOUNCE_MS) return;
    g_endstopPending = false;
    if (_cb) _cb();
}

bool Endstop::isTriggered() const {
    // NO: triggered = LOW (shorted to GND)
    // NC: triggered = HIGH (open circuit, pull-up wins)
    return (_mode == EndstopMode::NO)
        ? (digitalRead(PIN_ENDSTOP) == LOW)
        : (digitalRead(PIN_ENDSTOP) == HIGH);
}
