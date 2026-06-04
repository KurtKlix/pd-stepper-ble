#include "endstop.h"
#include "config.h"
#include "driver/gpio.h"

Endstop endstop;

volatile bool     g_endstopPending  = false;
volatile uint32_t g_endstopTime     = 0;
volatile uint32_t g_endstopIsrCount = 0;

// Post-callback lockout: ignores edges for ENDSTOP_LOCKOUT_MS after the callback fires.
// Prevents double-triggers from bounce on release or noise after the switch opens.
static volatile bool     s_locked     = false;
static volatile uint32_t s_lockStart  = 0;
#define ENDSTOP_LOCKOUT_MS 50

static void IRAM_ATTR onEndstopISR() {
    if (s_locked) return;
    if (!g_endstopPending) {
        g_endstopIsrCount++;
        g_endstopTime    = millis();
        g_endstopPending = true;
    }
}

void Endstop::init(EndstopCallback cb) {
    _cb = cb;
    // Use gpio_set_pull_mode for the strongest available pull-up (IDF level).
    // INPUT_PULLUP uses the same resistor but this makes the intent explicit.
    pinMode(PIN_ENDSTOP, INPUT_PULLUP);
    gpio_set_pull_mode((gpio_num_t)PIN_ENDSTOP, GPIO_PULLUP_ONLY);
    // NO mode default: switch shorts to GND → trigger on FALLING only.
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
    // Release lockout once the window has passed
    if (s_locked && millis() - s_lockStart >= ENDSTOP_LOCKOUT_MS) {
        s_locked = false;
    }
    if (!g_endstopPending) return;
    if (millis() - g_endstopTime < ENDSTOP_DEBOUNCE_MS) return;
    g_endstopPending = false;
    s_locked    = true;
    s_lockStart = millis();
    if (_cb) _cb();
}

bool Endstop::isTriggered() const {
    // NO: triggered = LOW (shorted to GND)
    // NC: triggered = HIGH (open circuit, pull-up wins)
    return (_mode == EndstopMode::NO)
        ? (digitalRead(PIN_ENDSTOP) == LOW)
        : (digitalRead(PIN_ENDSTOP) == HIGH);
}
