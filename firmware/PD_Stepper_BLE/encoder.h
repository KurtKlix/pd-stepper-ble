#pragma once
#include <Arduino.h>

class Encoder {
public:
    void init();
    void update();          // call every loop iteration
    void zero();            // set current position as new zero
    float getAbsoluteDeg() const;
    float getVelocityDps() const;

private:
    int32_t  _turnCount   = 0;
    uint16_t _rawAngle    = 0;
    uint16_t _lastRaw     = 0;
    uint16_t _offset      = 0;      // subtracted from raw before deg calc
    float    _absAngleDeg = 0.0f;
    float    _velDps      = 0.0f;
    uint32_t _lastUpdateMs = 0;

    uint16_t readRaw();
};

extern Encoder encoder;
