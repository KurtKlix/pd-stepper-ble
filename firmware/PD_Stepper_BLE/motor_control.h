#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

enum class MotorMode { IDLE, MOVING, VELOCITY, HOMING };

struct MotorConfig {
    int   currentMa      = 800;
    int   microsteps     = 16;
    int   speedSps       = 800;
    int   closedLoopType = 1;    // 0=open, 1=encoder-closed
    int   mappingDir     = 1;    // 1=normal, -1=reversed
    bool  enabled        = true;
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

private:
    void moveTo(float deg);
    void setVelocity(float dps);
    void stop();
    void startHoming();
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
    uint32_t _lastStepUs = 0;
    uint32_t _stepPeriodUs = 0; // microseconds between steps
};

extern MotorControl motorControl;
