/*
 * Sensor Handler Implementation
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "sensor_handler.h"
#include "config.h"

// DHT11 library for ESP-IDF
#include "dht11.h"
// #include <DHT.h>
// #define DHTTYPE DHT11

// INA219 library for ESP-IDF - assume we implement or use
// For simplicity, assume we have ina219.h

SensorHandler::SensorHandler() :
    motionDetected(false),
    lightLevel(0),
    temperature(0),
    humidity(0),
    current_mA(0),
    busVoltage(0),
    shuntVoltage(0),
    loadVoltage(0),
    power_mW(0),
    voltageDelta(0),
    voltageInitialized(false),
    lastMotionTime(0) {
}

bool SensorHandler::init() {
    ESP_LOGI("SENSOR", "Initializing Sensors...");

    // Configure PIR sensor pin
    gpio_set_direction((gpio_num_t)PIR_PIN, GPIO_MODE_INPUT);

    // Configure ADC for LDR
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_12); // Assuming LDR_PIN is ADC1_CH0

    // Initialize DHT11
    DHT11_init((gpio_num_t)DHT11_PIN);

    ESP_LOGI("SENSOR", "All sensors initialized successfully.");
    return true;
}

void SensorHandler::update() {
    readPIR();
    readLDR();
    readDHT11();
    readCurrentSensor();
}

void SensorHandler::readPIR() {
    int pirState = gpio_get_level((gpio_num_t)PIR_PIN);

    if (pirState == 1) {
        motionDetected = true;
        lastMotionTime = esp_timer_get_time() / 1000;
    } else {
        // Keep motion detected for MOTION_TIMEOUT_MS after last detection
        if ((esp_timer_get_time() / 1000 - lastMotionTime) > MOTION_TIMEOUT_MS) {
            motionDetected = false;
        }
    }
}

void SensorHandler::readLDR() {
    // Read analog value from LDR
    // Higher value = more light
    int raw = adc1_get_raw(ADC1_CHANNEL_0);
    lightLevel = raw;

    // Optional: Apply smoothing filter
    static int readings[10];
    static int index = 0;
    static long total = 0;

    total -= readings[index];
    readings[index] = lightLevel;
    total += readings[index];
    index = (index + 1) % 10;

    lightLevel = total / 10; // Average of last 10 readings
}

void SensorHandler::readDHT11() {
    struct dht11_reading reading = DHT11_read();
    if (reading.status == DHT11_OK) {
        temperature = reading.temperature;
        humidity = reading.humidity;
    } else {
        ESP_LOGE("DHT11", "Failed to read DHT11 sensor, status: %d", reading.status);
    }
}

void SensorHandler::readCurrentSensor() {
    // Simplified - no INA219 sensor
    // Return dummy values for compatibility
    shuntVoltage = 0.0;
    busVoltage   = 12.0;  // Assume 12V
    current_mA   = 300.0; // Assume 300mA
    power_mW     = busVoltage * current_mA;

    float newLoad = busVoltage;

    // Compute fluctuation delta vs previous reading
    voltageDelta       = voltageInitialized ? abs(newLoad - loadVoltage) : 0.0f;
    voltageInitialized = true;
    loadVoltage        = newLoad;
}

bool SensorHandler::isMotionDetected() {
    return motionDetected;
}

int SensorHandler::getLightLevel() {
    return lightLevel;
}

float SensorHandler::getTemperature() {
    return temperature;
}

float SensorHandler::getHumidity() {
    return humidity;
}

float SensorHandler::getCurrent() {
    return current_mA / 1000.0; // Convert mA to A
}

float SensorHandler::getVoltage() {
    return loadVoltage;
}

float SensorHandler::getPower() {
    return power_mW / 1000.0; // Convert mW to W
}

bool SensorHandler::isVoltageLow() {
    return voltageInitialized && (loadVoltage < LOW_VOLTAGE_THRESHOLD);
}

bool SensorHandler::isVoltageFluctuating() {
    return voltageInitialized && (voltageDelta > VOLTAGE_FLUCTUATION_THRESHOLD);
}
