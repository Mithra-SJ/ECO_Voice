/*
 * Sensor Handler Implementation
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
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

// INA219 register definitions (I2C address 0x40, 7-bit)
#define INA219_ADDR              0x40
#define INA219_REG_CONFIG        0x00
#define INA219_REG_SHUNTVOLTAGE  0x01
#define INA219_REG_BUSVOLTAGE    0x02
#define INA219_REG_POWER         0x03
#define INA219_REG_CURRENT       0x04
#define INA219_REG_CALIBRATION   0x05

// Config: 32V bus range, PGA /8 (±320mV shunt), 12-bit, continuous shunt+bus
#define INA219_CONFIG_VALUE      0x1FFF

// Calibration for 0.1Ω shunt, 1mA current LSB:
//   Cal = trunc(0.04096 / (0.001 * 0.1)) = 4096
#define INA219_CALIBRATION_VALUE 4096
#define INA219_CURRENT_LSB_A     0.001f  // 1 mA per bit

static esp_err_t ina219_write_reg(uint8_t reg, uint16_t value) {
    uint8_t data[3] = { reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA219_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, 3, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t ina219_read_reg(uint8_t reg, int16_t *out) {
    uint8_t data[2] = {0, 0};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA219_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);  // repeated start
    i2c_master_write_byte(cmd, (INA219_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_OK) {
        *out = (int16_t)((data[0] << 8) | data[1]);
    }
    return ret;
}

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
    // LDR_PIN = GPIO 8 → ADC1_CHANNEL_7 on ESP32-S3
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_12);

    // Initialize DHT11
    DHT11_init((gpio_num_t)DHT11_PIN);

    // Initialize I2C bus for INA219 (SDA=INA219_SDA, SCL=INA219_SCL)
    i2c_config_t i2c_cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = INA219_SDA,
        .scl_io_num       = INA219_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master           = { .clk_speed = 400000 },
        .clk_flags        = 0
    };
    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    esp_err_t i2c_err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (i2c_err != ESP_OK) {
        ESP_LOGE("SENSOR", "I2C init failed: %s", esp_err_to_name(i2c_err));
        return false;
    }

    // Configure INA219 — write calibration then config registers (non-fatal)
    if (ina219_write_reg(INA219_REG_CALIBRATION, INA219_CALIBRATION_VALUE) != ESP_OK ||
        ina219_write_reg(INA219_REG_CONFIG,      INA219_CONFIG_VALUE)      != ESP_OK) {
        ESP_LOGW("SENSOR", "INA219 init failed (addr=0x%02X). Check SDA=%d SCL=%d wiring. Current/voltage readings disabled.", INA219_ADDR, INA219_SDA, INA219_SCL);
        // Non-fatal — system continues without current sensor
    } else {
        ESP_LOGI("SENSOR", "INA219 initialized (addr=0x%02X).", INA219_ADDR);
    }

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
    // Read analog value from LDR — GPIO 8 = ADC1_CHANNEL_7 on ESP32-S3
    int raw = adc1_get_raw(ADC1_CHANNEL_7);

    // Smoothing filter — average of last 10 readings
    static int readings[10];
    static int index = 0;
    static long total = 0;
    static bool filled = false;

    // On first call, fill the entire buffer with the current reading to avoid
    // averaging with zeros during the first 10 ticks
    if (!filled) {
        for (int i = 0; i < 10; i++) readings[i] = raw;
        total = (long)raw * 10;
        filled = true;
    }

    total -= readings[index];
    readings[index] = raw;
    total += raw;
    index = (index + 1) % 10;

    lightLevel = total / 10;
}

void SensorHandler::readDHT11() {
    // DHT11 needs minimum ~1s between reads; hammering it causes timeout errors
    static uint32_t lastDhtRead = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    if (lastDhtRead != 0 && (now - lastDhtRead) < DHT11_READ_INTERVAL_MS) return;
    lastDhtRead = now;

    struct dht11_reading reading = DHT11_read();
    if (reading.status == DHT11_OK) {
        temperature = reading.temperature;
        humidity = reading.humidity;
    } else {
        ESP_LOGW("DHT11", "DHT11 read failed (status: %d), keeping previous values.", reading.status);
    }
}

void SensorHandler::readCurrentSensor() {
    int16_t raw = 0;

    // --- Bus Voltage (register 0x02) ---
    // Bits 15:3 hold the result; shift right 3, multiply by 4 mV/LSB
    if (ina219_read_reg(INA219_REG_BUSVOLTAGE, &raw) == ESP_OK) {
        float newVoltage = ((raw >> 3) * 4) / 1000.0f;  // mV → V
        voltageDelta       = voltageInitialized ? fabsf(newVoltage - loadVoltage) : 0.0f;
        voltageInitialized = true;
        loadVoltage        = newVoltage;
        busVoltage         = newVoltage;
    } else {
        ESP_LOGW("SENSOR", "INA219 bus voltage read failed.");
    }

    // --- Shunt Voltage (register 0x01) — 10 µV/LSB ---
    if (ina219_read_reg(INA219_REG_SHUNTVOLTAGE, &raw) == ESP_OK) {
        shuntVoltage = raw * 0.01f;  // LSB = 10 µV = 0.01 mV
    }

    // --- Current (register 0x04) — depends on calibration (1 mA/LSB) ---
    if (ina219_read_reg(INA219_REG_CURRENT, &raw) == ESP_OK) {
        current_mA = raw * (INA219_CURRENT_LSB_A * 1000.0f);  // A → mA
    }

    // --- Power (register 0x03) — 20 × current_LSB per bit ---
    if (ina219_read_reg(INA219_REG_POWER, &raw) == ESP_OK) {
        power_mW = raw * 20.0f * INA219_CURRENT_LSB_A * 1000.0f;  // → mW
    }
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

bool SensorHandler::isOvercurrent() {
    return (current_mA / 1000.0f) > OVERCURRENT_THRESHOLD;
}
