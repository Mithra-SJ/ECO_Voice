/*
 * Configuration File - ECO Voice System
 * Pin definitions, thresholds, and constants
 */

#ifndef CONFIG_H
#define CONFIG_H

// ===== PIN DEFINITIONS =====

// INMP441 I2S Microphone
#define I2S_SCK_PIN       5
#define I2S_WS_PIN        4
#define I2S_SD_PIN        6
#define I2S_PORT          I2S_NUM_0

// PIR Motion Sensor
#define PIR_PIN           7

// LDR (Light Dependent Resistor)
#define LDR_PIN           8   // ADC pin

// DHT11 Temperature & Humidity Sensor
#define DHT11_PIN         9

// INA219 Current Sensor (I2C)
#define INA219_SDA        1
#define INA219_SCL        2

// Relay Module
#define RELAY_LIGHT_PIN   17
#define RELAY_FAN_PIN     18

// Status LEDs
#define LED_GREEN_PIN     14  // System unlocked
#define LED_RED_PIN       13  // System locked

// DFPlayer Mini Audio Output
#define DFPLAYER_TX_PIN   16  // ESP32 TX → DFPlayer RX
#define DFPLAYER_RX_PIN   15  // ESP32 RX ← DFPlayer TX
#define DFPLAYER_BUSY_PIN 10  // DFPlayer BUSY → ESP32 (LOW=playing, HIGH=idle)

// ===== AUDIO SETTINGS =====
#define SAMPLE_RATE       16000
#define BITS_PER_SAMPLE   I2S_BITS_PER_SAMPLE_32BIT
#define I2S_CHANNEL       I2S_CHANNEL_FMT_ONLY_LEFT

// ===== THRESHOLDS =====
#define BRIGHTNESS_THRESHOLD          600   // ADC value (0-4095), higher = brighter
#define MOTION_TIMEOUT_MS             5000  // Motion detection persistence
#define WAKE_THRESHOLD                500   // Energy threshold for voice activity detection

// DHT11 — Fan control gate
#define TEMP_LOW_THRESHOLD            22.0  // °C  — below this, fan not recommended
#define HUMIDITY_LOW_THRESHOLD        40.0  // %RH — below this, fan not recommended

// INA219 — Voltage monitoring (informational notifications)
#define LOW_VOLTAGE_THRESHOLD         10.5  // Volts — below this, warn user
#define VOLTAGE_FLUCTUATION_THRESHOLD  1.5  // Volts delta between readings — warn user

// ===== AUTHENTICATION =====
#define AUTH_TIMEOUT_MS         30000 // 30 seconds to enter secret code
#define MAX_AUTH_ATTEMPTS       3     // Failed attempts before lockout

// ===== VOICE RECOGNITION SETTINGS =====
#define WAKE_WORD               "hi esp"
#define WAKE_CONFIDENCE         0.7   // Confidence threshold (0.0 - 1.0)
#define COMMAND_TIMEOUT_MS      5000  // Time to wait for command after wake
#define YES_NO_TIMEOUT_MS       10000 // Time to wait for yes/no response

// ===== SYSTEM SETTINGS =====
#define SERIAL_BAUD_RATE        115200
#define UNLOCK_TIMEOUT_MS       300000 // Auto-lock after 5 min of inactivity

#endif // CONFIG_H
