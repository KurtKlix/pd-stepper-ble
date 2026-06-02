#include "motor_control.h"
#include "config.h"
#include "encoder.h"
#include "endstop.h"
#include <TMCStepper.h>

MotorControl motorControl;

// TMC2209 driver instance (UART interface)
static TMC2209Stepper driver(&Serial1, 0.11f, 0b00); // R_sense=0.11, addr=0

static void applyMicrosteps(int ms) {
    // TMC2209 UART microstep configuration
    driver.microsteps(ms);
    // Also set MS1/MS2 pins for standalone fallback
    // (TMC2209 ignores these when UART is active, but good practice)
}

void MotorControl::init(MotorEventCallback cb) {
    _eventCb = cb;

    // USB PD voltage negotiation — must be set before enabling motor
    pinMode(PIN_CFG1, OUTPUT);
    pinMode(PIN_CFG2, OUTPUT);
    pinMode(PIN_CFG3, OUTPUT);
    // 20V: CFG1=LOW, CFG2=HIGH, CFG3=LOW
    digitalWrite(PIN_CFG1, LOW);
    digitalWrite(PIN_CFG2, HIGH);
    digitalWrite(PIN_CFG3, LOW);
    delay(100); // allow PD negotiation to complete

    // Step/Dir/Enable pins
    pinMode(PIN_STEP,   OUTPUT);
    pinMode(PIN_DIR,    OUTPUT);
    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, HIGH); // HIGH = disabled initially

    // Microstepping pins
    pinMode(PIN_MS1, OUTPUT);
    pinMode(PIN_MS2, OUTPUT);

    // TMC2209 UART
    Serial1.begin(TMC_SERIAL_BAUD, SERIAL_8N1, PIN_TMC_RX, PIN_TMC_TX);
    driver.begin();
    driver.toff(5);
    driver.blank_time(24);
    driver.rms_current(_cfg.currentMa);
    driver.microsteps(_cfg.microsteps);
    driver.pwm_autoscale(true);
    driver.SGTHRS(50);  // StallGuard threshold
    // Reduce current to DEFAULT_HOLD_CURRENT_MA when motor is idle.
    // IHOLDDELAY: time to ramp down after move (units of 2^18 clock cycles)
    driver.iholddelay(10);
    driver.ihold((DEFAULT_HOLD_CURRENT_MA * 31) / _cfg.currentMa); // 0-31 scale

    applyConfig();
}

void MotorControl::applyConfig() {
    driver.rms_current(_cfg.currentMa);
    applyMicrosteps(_cfg.microsteps);

    // Step period from speedSps and microsteps
    if (_cfg.speedSps > 0) {
        _stepPeriodUs = 1000000UL / (uint32_t)(_cfg.speedSps);
    }

    digitalWrite(PIN_ENABLE, _cfg.enabled ? LOW : HIGH);
}

void MotorControl::controlLoop() {
    if (_mode == MotorMode::IDLE) return;

    float curDeg = encoder.getAbsoluteDeg();

    if (_mode == MotorMode::VELOCITY) {
        // Continuous step generation at _velDps rate
        // stepPeriodUs is recalculated when velocity is set
        uint32_t now = micros();
        if (_stepPeriodUs > 0 && (now - _lastStepUs) >= _stepPeriodUs) {
            _lastStepUs = now;
            digitalWrite(PIN_STEP, HIGH);
            delayMicroseconds(2);
            digitalWrite(PIN_STEP, LOW);
        }
        return;
    }

    if (_mode == MotorMode::HOMING) {
        // Velocity-driven; endstop ISR calls motorControl's stop + zero path
        uint32_t now = micros();
        if (_stepPeriodUs > 0 && (now - _lastStepUs) >= _stepPeriodUs) {
            _lastStepUs = now;
            digitalWrite(PIN_STEP, HIGH);
            delayMicroseconds(2);
            digitalWrite(PIN_STEP, LOW);
        }
        return;
    }

    if (_mode == MotorMode::MOVING) {
        float error = _targetDeg - curDeg;
        // Wrap error to [-180, +180] so the controller always takes the
        // shortest arc, e.g. treats 359° as -1° when target is 0°.
        while (error >  180.0f) error -= 360.0f;
        while (error < -180.0f) error += 360.0f;
        float absErr = fabsf(error);

        if (absErr < POSITION_THRESHOLD_DEG) {
            if (!_inDwell) {
                _inDwell    = true;
                _dwellStart = millis();
            } else if (millis() - _dwellStart >= POSITION_DWELL_MS) {
                // Position reached
                _mode    = MotorMode::IDLE;
                _inDwell = false;
                digitalWrite(PIN_ENABLE, _cfg.enabled ? LOW : HIGH);

                char buf[128];
                snprintf(buf, sizeof(buf),
                    "{\"type\":\"position_reached\",\"pos_deg\":%.2f,\"target_deg\":%.2f,\"ts\":%lu}",
                    curDeg, _targetDeg, millis());
                if (_eventCb) _eventCb(buf);
                return;
            }
        } else {
            _inDwell = false;
            // Only step when outside the threshold — stops overshooting
            stepToward(_targetDeg);
        }
    }
}

void MotorControl::stepToward(float targetDeg) {
    float curDeg = encoder.getAbsoluteDeg();
    float error  = targetDeg - curDeg;
    while (error >  180.0f) error -= 360.0f;
    while (error < -180.0f) error += 360.0f;

    // Direction
    bool forward = (error * (float)_cfg.mappingDir) > 0;
    digitalWrite(PIN_DIR, forward ? HIGH : LOW);

    uint32_t now = micros();
    if (_stepPeriodUs > 0 && (now - _lastStepUs) >= _stepPeriodUs) {
        _lastStepUs = now;
        digitalWrite(PIN_STEP, HIGH);
        delayMicroseconds(2);
        digitalWrite(PIN_STEP, LOW);
    }
}

void MotorControl::moveTo(float deg) {
    _targetDeg   = deg;
    _mode        = MotorMode::MOVING;
    _inDwell     = false;
    _stepPeriodUs = (_cfg.speedSps > 0) ? (1000000UL / (uint32_t)_cfg.speedSps) : 0;
    digitalWrite(PIN_ENABLE, LOW);
}

void MotorControl::setVelocity(float dps) {
    if (fabsf(dps) < 0.01f) {
        stop();
        return;
    }

    // Convert dps to step frequency
    // dps * (stepsPerRev * microsteps) / 360
    float stepsPerDeg = (float)(DEFAULT_STEPS_PER_REV * _cfg.microsteps) / 360.0f;
    float stepsPerSec = fabsf(dps) * stepsPerDeg;
    _stepPeriodUs = (stepsPerSec > 0) ? (uint32_t)(1000000.0f / stepsPerSec) : 0;
    _velDps = dps;

    bool forward = (dps * (float)_cfg.mappingDir) > 0;
    digitalWrite(PIN_DIR, forward ? HIGH : LOW);
    digitalWrite(PIN_ENABLE, LOW);
    _mode = MotorMode::VELOCITY;
}

void MotorControl::stop() {
    _mode    = MotorMode::IDLE;
    _inDwell = false;
    _velDps  = 0.0f;
    // Keep enable state as configured
}

void MotorControl::startHoming() {
    // Drive slowly toward endstop (negative direction by convention)
    float homingDps = -(360.0f * DEFAULT_HOMING_SPEED)
                      / (float)(DEFAULT_STEPS_PER_REV * _cfg.microsteps);
    float stepsPerSec = (float)DEFAULT_HOMING_SPEED;
    _stepPeriodUs = (uint32_t)(1000000.0f / stepsPerSec);
    _mode = MotorMode::HOMING;
    bool forward = (_cfg.mappingDir < 0); // reverse to go toward endstop
    digitalWrite(PIN_DIR, forward ? HIGH : LOW);
    digitalWrite(PIN_ENABLE, LOW);
}

void MotorControl::applyCommand(JsonDocument& doc) {
    const char* cmd = doc["cmd"] | "";

    if (strcmp(cmd, "deg") == 0) {
        moveTo(doc["val"].as<float>());

    } else if (strcmp(cmd, "deg_rel") == 0) {
        moveTo(encoder.getAbsoluteDeg() + doc["val"].as<float>());

    } else if (strcmp(cmd, "vel") == 0) {
        setVelocity(doc["val"].as<float>());

    } else if (strcmp(cmd, "stop") == 0) {
        stop();

    } else if (strcmp(cmd, "home") == 0) {
        startHoming();

    } else if (strcmp(cmd, "speed") == 0) {
        _cfg.speedSps = doc["val"].as<int>();
        if (_stepPeriodUs > 0 && _mode == MotorMode::MOVING) {
            _stepPeriodUs = 1000000UL / (uint32_t)_cfg.speedSps;
        }

    } else if (strcmp(cmd, "microsteps") == 0) {
        _cfg.microsteps = doc["val"].as<int>();
        applyMicrosteps(_cfg.microsteps);

    } else if (strcmp(cmd, "current") == 0) {
        _cfg.currentMa = doc["val"].as<int>();
        driver.rms_current(_cfg.currentMa);

    } else if (strcmp(cmd, "enable") == 0) {
        _cfg.enabled = doc["val"].as<int>() != 0;
        digitalWrite(PIN_ENABLE, _cfg.enabled ? LOW : HIGH);

    } else if (strcmp(cmd, "closed_loop_type") == 0) {
        _cfg.closedLoopType = doc["val"].as<int>();

    } else if (strcmp(cmd, "mappingDirection") == 0) {
        _cfg.mappingDir = doc["val"].as<int>();

    } else if (strcmp(cmd, "endstop_mode") == 0) {
        // 0 = NO (normally-open, active-low)
        // 1 = NC (normally-closed, active-high)
        endstop.setMode(doc["val"].as<int>() == 0
                        ? EndstopMode::NO
                        : EndstopMode::NC);
    }
}
