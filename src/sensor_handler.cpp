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
#define INA219_ADDR_MIN          0x40
#define INA219_ADDR_MAX          0x4F
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

static uint8_t s_ina219_addr = INA219_ADDR;

static esp_err_t ina219_write_reg(uint8_t reg, uint16_t value) {
    uint8_t data[3] = { reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (s_ina219_addr << 1) | I2C_MASTER_WRITE, true);
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
    i2c_master_write_byte(cmd, (s_ina219_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);  // repeated start
    i2c_master_write_byte(cmd, (s_ina219_addr << 1) | I2C_MASTER_READ, true);
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

static esp_err_t i2c_probe_addr(uint8_t addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static bool scan_for_ina219_addr(uint8_t *found_addr) {
    bool found = false;

    ESP_LOGI("SENSOR", "Scanning I2C bus on SDA=%d SCL=%d...", INA219_SDA, INA219_SCL);
    for (uint8_t addr = INA219_ADDR_MIN; addr <= INA219_ADDR_MAX; ++addr) {
        esp_err_t ret = i2c_probe_addr(addr);
        if (ret == ESP_OK) {
            ESP_LOGI("SENSOR", "I2C device detected at 0x%02X", addr);
            if (!found) {
                *found_addr = addr;
                found = true;
            }
        }
    }

    if (!found) {
        ESP_LOGW("SENSOR", "No I2C device found in INA219 address range 0x%02X-0x%02X",
                 INA219_ADDR_MIN, INA219_ADDR_MAX);
    }

    return found;
}

SensorHandler::SensorHandler() :
    motionDetected(false),
    ina219Available(false),
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
        .master           = { .clk_speed = 100000 },
        .clk_flags        = 0
    };
    esp_err_t param_err = i2c_param_config(I2C_NUM_0, &i2c_cfg);
    if (param_err != ESP_OK) {
        ESP_LOGE("SENSOR", "I2C param config failed: %s", esp_err_to_name(param_err));
        return false;
    }

    esp_err_t i2c_err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (i2c_err != ESP_OK && i2c_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE("SENSOR", "I2C init failed: %s", esp_err_to_name(i2c_err));
        return false;
    }

    // Configure INA219 — write calibration then config registers
    if (scan_for_ina219_addr(&s_ina219_addr)) {
        esp_err_t cal_err = ina219_write_reg(INA219_REG_CALIBRATION, INA219_CALIBRATION_VALUE);
        esp_err_t cfg_err = (cal_err == ESP_OK) ? ina219_write_reg(INA219_REG_CONFIG, INA219_CONFIG_VALUE) : cal_err;

        if (cal_err == ESP_OK && cfg_err == ESP_OK) {
            ina219Available = true;
            ESP_LOGI("SENSOR", "INA219 initialized at 0x%02X.", s_ina219_addr);
        } else {
            ESP_LOGE("SENSOR", "INA219 init failed at 0x%02X. calibration=%s config=%s",
                     s_ina219_addr, esp_err_to_name(cal_err), esp_err_to_name(cfg_err));
            ESP_LOGW("SENSOR", "Continuing without INA219 measurements.");
        }
    } else {
        ESP_LOGW("SENSOR", "INA219 not detected on the I2C bus. Continuing without current sensing.");
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
    if (!ina219Available) {
        return;
    }

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
