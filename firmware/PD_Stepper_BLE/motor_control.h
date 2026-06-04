#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

enum class MotorMode  { IDLE, MOVING, VELOCITY, HOMING, SENSORLESS_HOMING };
enum class HomingMode { ENDSTOP = 0, SENSORLESS = 1 };

struct MotorConfig {
    int        currentMa         = DEFAULT_CURRENT_MA;
    int        microsteps        = DEFAULT_MICROSTEPS;
    int        speedSps          = DEFAULT_SPEED_SPS;
    int        closedLoopType    = 1;
    int        mappingDir        = 1;
    int        homeDir           = DEFAULT_HOMING_DIR;
    HomingMode homingMode        = HomingMode::ENDSTOP;
    int        sensorlessCurMa   = DEFAULT_SENSORLESS_CUR_MA;
    int        sgthrs            = DEFAULT_SGTHRS;
    int        sensorlessSpeedSps= DEFAULT_SENSORLESS_SPEED;
    bool       enabled           = true;
};

typedef void (*MotorEventCallback)(const char* eventJson);

class MotorControl {
public:
    void init(MotorEventCallback cb);
    void controlLoop();           // call every loop iteration
    void applyCommand(JsonDocument& doc);

    MotorMode    getMode()      const { return _mode; }
    float        getTargetDeg() const { return _targetDeg; }
    MotorConfig& getConfig()          { return _cfg; }
    int          getSgResult();       // live StallGuard reading from TMC2209 (0-510, lower = more load)

private:
    void moveTo(float deg);
    void setVelocity(float dps);
    void stop();
    void startHoming();
    void startEndstopHoming();
    void startSensorlessHoming();
    void applyConfig();

    // Called by encoder-closed-loop position step
    void stepToward(float targetDeg);

    MotorConfig       _cfg;
    MotorMode         _mode      = MotorMode::IDLE;
    float             _targetDeg = 0.0f;
    float             _velDps    = 0.0f;
    uint32_t          _dwellStart = 0;
    bool              _inDwell   = false;
    MotorEventCallback _eventCb  = nullptr;

    // Step generation
    uint32_t _lastStepUs    = 0;
    uint32_t _stepPeriodUs  = 0;

    // Sensorless homing settle + SG polling
    uint32_t _sgSettleStart  = 0;
    bool     _sgSettled      = false;
    uint32_t _lastSgMs       = 0;
    int      _lastSgVal      = 510;  // cached SG_RESULT; 510 = max (not stalled)
    uint8_t  _sgLowCount     = 0;   // consecutive reads below threshold before stall fires
};

extern MotorControl motorControl;
