/*
 * Configuration File - ECO Voice System
 * Pin definitions, thresholds, and constants
 */

#ifndef CONFIG_H
#define CONFIG_H

// ===== PIN DEFINITIONS =====

// INMP441 I2S Microphone
#define I2S_SCK_PIN       41
#define I2S_WS_PIN        42
#define I2S_SD_PIN        2
#define I2S_PORT          I2S_NUM_0

// PIR Motion Sensor
#define PIR_PIN           4

// LDR (Light Dependent Resistor)
#define LDR_PIN           1   // ADC pin

// INA219 Current Sensor (I2C)
#define INA219_SDA        8
#define INA219_SCL        9

// Relay Module
#define RELAY_LIGHT_PIN   10
#define RELAY_FAN_PIN     11

// Status LEDs
#define LED_GREEN_PIN     12  // System unlocked
#define LED_RED_PIN       13  // System locked

// ===== AUDIO SETTINGS =====
#define SAMPLE_RATE       16000
#define BITS_PER_SAMPLE   I2S_BITS_PER_SAMPLE_32BIT
#define I2S_CHANNEL       I2S_CHANNEL_FMT_ONLY_LEFT

// ===== THRESHOLDS =====
#define BRIGHTNESS_THRESHOLD    600   // ADC value (0-4095), higher = brighter
#define CURRENT_THRESHOLD       2.5   // Amps
#define MOTION_TIMEOUT_MS       5000  // Motion detection persistence

// ===== AUTHENTICATION =====
#define AUTH_TIMEOUT_MS         30000 // 30 seconds
#define SECRET_CODE             "open sesame" // Change this to your secret phrase

// ===== VOICE RECOGNITION SETTINGS =====
#define WAKE_WORD               "eco"
#define WAKE_CONFIDENCE         0.7   // Confidence threshold (0.0 - 1.0)
#define COMMAND_TIMEOUT_MS      5000  // Time to wait for command after wake

// ===== SYSTEM SETTINGS =====
#define SERIAL_BAUD_RATE        115200
#define RESPONSE_DELAY_MS       500   // Delay before responding

#endif // CONFIG_H
