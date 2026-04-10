/*
 * config.h — ECO Voice Online Version
 * Pin definitions and system thresholds
 */

#ifndef CONFIG_H
#define CONFIG_H

// ===== I2C (INA219 Current Sensor) =====
#define INA219_SDA        1
#define INA219_SCL        2

// ===== SENSORS =====
#define PIR_PIN           7
#define LDR_PIN           8
#define DHT11_PIN         9

// ===== RELAYS =====
#define RELAY_LIGHT_PIN   17
#define RELAY_FAN_PIN     18
#define RELAY_ACTIVE_LOW  1    // relay triggers on LOW

// ===== STATUS LEDs =====
#define LED_GREEN_PIN     14   // device online
#define LED_RED_PIN       13   // device offline / error

// ===== SENSOR THRESHOLDS =====
#define BRIGHTNESS_THRESHOLD          600    // ADC 0-4095 — above = already bright
#define TEMP_LOW_THRESHOLD            22.0f  // °C  — below = fan not recommended
#define HUMIDITY_LOW_THRESHOLD        40.0f  // %RH — below = fan not recommended
#define LOW_VOLTAGE_THRESHOLD         4.5f   // V   — below = warn user
#define VOLTAGE_FLUCTUATION_THRESHOLD 0.3f   // V delta between readings
#define MOTION_TIMEOUT_MS             5000   // ms — how long motion stays active after trigger

// ===== I2S MICROPHONE (INMP441) =====
#define I2S_SCK_PIN   5
#define I2S_WS_PIN    4
#define I2S_SD_PIN    6

// ===== VOICE RECOGNITION =====
#define VOICE_COMMAND_TIMEOUT_MS  10000  // ms to listen for command after wake word

// ===== FIREBASE SYNC INTERVALS =====
#define SENSOR_PUSH_INTERVAL_MS   2000   // push sensor data every 2 seconds
#define COMMAND_POLL_INTERVAL_MS  500    // check for new commands every 500ms

#endif // CONFIG_H
