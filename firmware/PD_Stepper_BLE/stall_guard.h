#pragma once
#include <Arduino.h>

typedef void (*StallCallback)();

class StallGuard {
public:
    void init(StallCallback cb);
    void process();  // call every loop iteration

private:
    StallCallback _cb = nullptr;
};

extern StallGuard stallGuard;

// ISR-safe flag written by interrupt, read in process()
extern volatile bool     g_stallPending;
extern volatile uint32_t g_stallTime;
