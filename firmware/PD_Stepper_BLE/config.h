#pragma once

// ── Motor driver (TMC2209) ──────────────────────────────────────────────────
#define PIN_STEP        5
#define PIN_DIR         6
#define PIN_ENABLE      21
#define PIN_DIAG        16   // StallGuard output (open-drain, active HIGH)
#define PIN_MS1         1
#define PIN_MS2         2
#define PIN_TMC_TX      17
#define PIN_TMC_RX      18
#define TMC_SERIAL_BAUD 115200

// ── Encoder (AS5600, I2C) ───────────────────────────────────────────────────
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9
#define AS5600_ADDR     0x36
#define ENCODER_UPDATE_MS 10   // poll interval

// ── Endstop ─────────────────────────────────────────────────────────────────
// AUX connector GPIO 13 (AUX_RX pin repurposed when BLE mode active)
#define PIN_ENDSTOP     13
#define ENDSTOP_DEBOUNCE_MS 2

// ── Stall guard ─────────────────────────────────────────────────────────────
#define STALL_DEBOUNCE_MS   50

// ── Position control ─────────────────────────────────────────────────────────
#define POSITION_THRESHOLD_DEG  0.5f   // within this = "at target"
#define POSITION_DWELL_MS       150    // must hold threshold for this long

// ── USB Power Delivery (CH224K) ──────────────────────────────────────────────
#define PIN_CFG1        38
#define PIN_CFG2        48
#define PIN_CFG3        47
#define PIN_VBUS        4                  // ADC input through 20k/2.7k divider
#define VBUS_DIV_RATIO  0.1189427313f      // 2.7 / (20 + 2.7)
// 20V: CFG1=LOW, CFG2=HIGH, CFG3=LOW
// 15V: CFG1=LOW, CFG2=HIGH, CFG3=HIGH
// 12V: CFG1=LOW, CFG2=LOW,  CFG3=HIGH
//  9V: CFG1=LOW, CFG2=LOW,  CFG3=LOW
#define PD_VOLTAGE      20   // requested USB PD voltage

// ── Default motor settings ───────────────────────────────────────────────────
#define DEFAULT_CURRENT_MA      800
#define DEFAULT_HOLD_CURRENT_MA 200    // standstill current (reduces heat)
#define DEFAULT_MICROSTEPS      16
#define DEFAULT_SPEED_SPS       800    // steps/sec for position moves
#define DEFAULT_HOMING_SPEED        200    // steps/sec during endstop homing
#define DEFAULT_HOMING_DIR          (-1)   // -1 = negative direction, 1 = positive
#define DEFAULT_SENSORLESS_SPEED    1200   // steps/sec during sensorless homing
#define DEFAULT_SENSORLESS_CUR_MA   800    // run current during sensorless homing
#define DEFAULT_SGTHRS              8      // StallGuard threshold (0-255, higher = more sensitive, fires when SG_RESULT < SGTHRS*2)
#define DEFAULT_STEPS_PER_REV   200

// ── BLE ──────────────────────────────────────────────────────────────────────
#define BLE_DEVICE_NAME         "PD-Stepper"
#define BLE_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DC4179"
#define BLE_CMD_UUID            "6E400002-B5A3-F393-E0A9-E50E24DC4179"
#define BLE_STATUS_UUID         "6E400003-B5A3-F393-E0A9-E50E24DC4179"
#define BLE_CONFIG_UUID         "6E400004-B5A3-F393-E0A9-E50E24DC4179"
#define BLE_STATUS_INTERVAL_MS  200    // periodic status notify rate

// ── Event queue ──────────────────────────────────────────────────────────────
#define EVENT_QUEUE_SIZE        8      // max pending BLE events
#define FW_VERSION              "1.0.0"
