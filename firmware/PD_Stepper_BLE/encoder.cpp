#include "encoder.h"
#include "config.h"
#include <Wire.h>

Encoder encoder;

void Encoder::init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    _lastRaw = readRaw();
    _offset  = _lastRaw;
    _lastUpdateMs = millis();
}

void Encoder::update() {
    uint32_t now = millis();
    if (now - _lastUpdateMs < ENCODER_UPDATE_MS) return;

    uint16_t raw = readRaw();

    // Detect wraparound by looking for jumps larger than half the range.
    // This works regardless of where the zero offset sits relative to the
    // physical 0/4096 boundary.
    int16_t diff = (int16_t)raw - (int16_t)_lastRaw;
    if (diff < -2048) {
        _turnCount++;   // raw jumped from near 4095 down to near 0
    } else if (diff > 2048) {
        _turnCount--;   // raw jumped from near 0 up to near 4095
    }

    _lastRaw = raw;

    // Compute absolute angle relative to zero offset
    int32_t adjusted = (int32_t)raw - (int32_t)_offset;
    if (adjusted < 0) adjusted += 4096;
    float newDeg = (float)_turnCount * 360.0f + ((float)adjusted / 4096.0f) * 360.0f;

    float dt = (float)(now - _lastUpdateMs) / 1000.0f;
    if (dt > 0.0f) {
        _velDps = (newDeg - _absAngleDeg) / dt;
    }

    _absAngleDeg  = newDeg;
    _lastUpdateMs = now;
}

void Encoder::zero() {
    _turnCount   = 0;
    _offset      = _lastRaw;
    _absAngleDeg = 0.0f;
    _velDps      = 0.0f;
}

float Encoder::getAbsoluteDeg() const { return _absAngleDeg; }
float Encoder::getVelocityDps() const { return _velDps; }

uint16_t Encoder::readRaw() {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(0x0C); // RAW_ANGLE high byte register
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
    if (Wire.available() < 2) return _lastRaw;
    uint16_t hi = Wire.read();
    uint16_t lo = Wire.read();
    return ((hi << 8) | lo) & 0x0FFF;
}
