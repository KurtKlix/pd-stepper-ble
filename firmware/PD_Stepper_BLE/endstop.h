#pragma once
#include <Arduino.h>

typedef void (*EndstopCallback)();

// NO (normally-open): switch connects signal to GND when triggered → FALLING edge
// NC (normally-closed): switch opens when triggered → RISING edge
enum class EndstopMode { NO = 0, NC = 1 };

class Endstop {
public:
    void init(EndstopCallback cb);
    void process();
    bool isTriggered() const;
    void setMode(EndstopMode mode);   // reconfigure at runtime, no reflash needed
    EndstopMode getMode() const { return _mode; }

private:
    EndstopCallback _cb   = nullptr;
    EndstopMode     _mode = EndstopMode::NO;
};

extern Endstop endstop;

extern volatile bool     g_endstopPending;
extern volatile uint32_t g_endstopTime;
extern volatile uint32_t g_endstopIsrCount;  // total ISR fires — for debug
