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

#include "dht11.h"

#define INA219_ADDR              0x40
#define INA219_ADDR_MIN          0x40
#define INA219_ADDR_MAX          0x4F
#define INA219_REG_CONFIG        0x00
#define INA219_REG_SHUNTVOLTAGE  0x01
#define INA219_REG_BUSVOLTAGE    0x02
#define INA219_REG_POWER         0x03
#define INA219_REG_CURRENT       0x04
#define INA219_REG_CALIBRATION   0x05

#define INA219_CONFIG_VALUE      0x1FFF
#define INA219_CALIBRATION_VALUE 4096
#define INA219_CURRENT_LSB_A     0.001f

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
    i2c_master_start(cmd);
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
    dht11Available(true),
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
    lastCurrent_A(0),
    currentDelta_A(0),
    currentInitialized(false),
    dht11FailureCount(0),
    lastDht11ReadMs(0),
    lastDht11LogMs(0),
    motionOverrideEnabled(false),
    motionOverrideValue(false),
    lightOverrideEnabled(false),
    lightOverrideValue(0),
    temperatureOverrideEnabled(false),
    temperatureOverrideValue(0),
    humidityOverrideEnabled(false),
    humidityOverrideValue(0),
    voltageOverrideEnabled(false),
    voltageOverrideValue(0),
    currentOverrideEnabled(false),
    currentOverrideValue_A(0),
    powerOverrideEnabled(false),
    powerOverrideValue_W(0),
    lastMotionTime(0) {
}

bool SensorHandler::init() {
    ESP_LOGI("SENSOR", "Initializing Sensors...");

    gpio_set_direction((gpio_num_t)PIR_PIN, GPIO_MODE_INPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_12);

    DHT11_init((gpio_num_t)DHT11_PIN);

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
    if (motionOverrideEnabled) {
        motionDetected = motionOverrideValue;
        return;
    }

    int pirState = gpio_get_level((gpio_num_t)PIR_PIN);

    if (pirState == 1) {
        motionDetected = true;
        lastMotionTime = esp_timer_get_time() / 1000;
    } else if ((esp_timer_get_time() / 1000 - lastMotionTime) > MOTION_TIMEOUT_MS) {
        motionDetected = false;
    }
}

void SensorHandler::readLDR() {
    if (lightOverrideEnabled) {
        lightLevel = lightOverrideValue;
        return;
    }

    int raw = adc1_get_raw(ADC1_CHANNEL_7);
    lightLevel = raw;

    static int readings[10] = {0};
    static int index = 0;
    static long total = 0;

    total -= readings[index];
    readings[index] = lightLevel;
    total += readings[index];
    index = (index + 1) % 10;

    lightLevel = total / 10;
}

void SensorHandler::readDHT11() {
    if (temperatureOverrideEnabled || humidityOverrideEnabled) {
        if (temperatureOverrideEnabled) {
            temperature = temperatureOverrideValue;
        }
        if (humidityOverrideEnabled) {
            humidity = humidityOverrideValue;
        }
        dht11Available = true;
        return;
    }

    constexpr int kDht11ReadIntervalMs = 2000;
    constexpr int kDht11LogIntervalMs = 10000;
    constexpr int kDht11DisableAfterFailures = 5;

    int64_t nowMs = esp_timer_get_time() / 1000;
    if ((nowMs - lastDht11ReadMs) < kDht11ReadIntervalMs) {
        return;
    }
    lastDht11ReadMs = nowMs;

    struct dht11_reading reading = DHT11_read();
    if (reading.status == DHT11_OK) {
        temperature = reading.temperature;
        humidity = reading.humidity;
        if (dht11FailureCount > 0) {
            ESP_LOGI("DHT11", "DHT11 communication restored.");
        }
        dht11FailureCount = 0;
        dht11Available = true;
    } else {
        ++dht11FailureCount;

        if (!dht11Available) {
            return;
        }

        if (dht11FailureCount >= kDht11DisableAfterFailures) {
            dht11Available = false;
            ESP_LOGW("DHT11", "DHT11 read failed %d times. Disabling further DHT11 logs until reboot.",
                     dht11FailureCount);
            return;
        }

        if (dht11FailureCount == 1 || (nowMs - lastDht11LogMs) >= kDht11LogIntervalMs) {
            lastDht11LogMs = nowMs;
            ESP_LOGW("DHT11", "DHT11 read failed, status=%d (attempt %d/%d).",
                     reading.status, dht11FailureCount, kDht11DisableAfterFailures);
        }
    }
}

void SensorHandler::readCurrentSensor() {
    if (voltageOverrideEnabled || currentOverrideEnabled || powerOverrideEnabled) {
        if (voltageOverrideEnabled) {
            voltageInitialized = true;
            loadVoltage = voltageOverrideValue;
            busVoltage = voltageOverrideValue;
        }
        if (currentOverrideEnabled) {
            current_mA = currentOverrideValue_A * 1000.0f;
            currentDelta_A = currentInitialized ? fabsf(currentOverrideValue_A - lastCurrent_A) : 0.0f;
            lastCurrent_A = currentOverrideValue_A;
            currentInitialized = true;
        }
        if (powerOverrideEnabled) {
            power_mW = powerOverrideValue_W * 1000.0f;
        }
        ina219Available = true;
        return;
    }

    if (!ina219Available) {
        return;
    }

    int16_t raw = 0;

    if (ina219_read_reg(INA219_REG_BUSVOLTAGE, &raw) == ESP_OK) {
        float newVoltage = ((raw >> 3) * 4) / 1000.0f;
        voltageDelta = voltageInitialized ? fabsf(newVoltage - loadVoltage) : 0.0f;
        voltageInitialized = true;
        loadVoltage = newVoltage;
        busVoltage = newVoltage;
    } else {
        ESP_LOGW("SENSOR", "INA219 bus voltage read failed.");
    }

    if (ina219_read_reg(INA219_REG_SHUNTVOLTAGE, &raw) == ESP_OK) {
        shuntVoltage = raw * 0.01f;
    }

    if (ina219_read_reg(INA219_REG_CURRENT, &raw) == ESP_OK) {
        const float currentA = raw * INA219_CURRENT_LSB_A;
        currentDelta_A = currentInitialized ? fabsf(currentA - lastCurrent_A) : 0.0f;
        currentInitialized = true;
        lastCurrent_A = currentA;
        current_mA = currentA * 1000.0f;
    }

    if (ina219_read_reg(INA219_REG_POWER, &raw) == ESP_OK) {
        power_mW = raw * 20.0f * INA219_CURRENT_LSB_A * 1000.0f;
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
    return current_mA / 1000.0f;
}

float SensorHandler::getVoltage() {
    return loadVoltage;
}

float SensorHandler::getPower() {
    return power_mW / 1000.0f;
}

bool SensorHandler::isVoltageLow() {
    return voltageInitialized && (loadVoltage < LOW_VOLTAGE_THRESHOLD);
}

bool SensorHandler::isVoltageFluctuating() {
    return voltageInitialized && (voltageDelta > VOLTAGE_FLUCTUATION_THRESHOLD);
}

bool SensorHandler::isCurrentFluctuating() {
    return currentInitialized && (currentDelta_A > CURRENT_FLUCTUATION_THRESHOLD);
}

bool SensorHandler::isOvercurrent() {
    return currentInitialized && (getCurrent() > OVERCURRENT_THRESHOLD);
}

bool SensorHandler::isIna219Available() const {
    return ina219Available || voltageOverrideEnabled || currentOverrideEnabled || powerOverrideEnabled;
}

bool SensorHandler::isDht11Available() const {
    return dht11Available || temperatureOverrideEnabled || humidityOverrideEnabled;
}

void SensorHandler::clearOverrides() {
    motionOverrideEnabled = false;
    lightOverrideEnabled = false;
    temperatureOverrideEnabled = false;
    humidityOverrideEnabled = false;
    voltageOverrideEnabled = false;
    currentOverrideEnabled = false;
    powerOverrideEnabled = false;
}

void SensorHandler::setMotionOverride(bool enabled, bool value) {
    motionOverrideEnabled = enabled;
    motionOverrideValue = value;
}

void SensorHandler::setLightLevelOverride(bool enabled, int value) {
    lightOverrideEnabled = enabled;
    lightOverrideValue = value;
}

void SensorHandler::setTemperatureOverride(bool enabled, float value) {
    temperatureOverrideEnabled = enabled;
    temperatureOverrideValue = value;
}

void SensorHandler::setHumidityOverride(bool enabled, float value) {
    humidityOverrideEnabled = enabled;
    humidityOverrideValue = value;
}

void SensorHandler::setVoltageOverride(bool enabled, float value) {
    voltageOverrideEnabled = enabled;
    voltageOverrideValue = value;
}

void SensorHandler::setCurrentOverride(bool enabled, float valueAmps) {
    currentOverrideEnabled = enabled;
    currentOverrideValue_A = valueAmps;
}

void SensorHandler::setPowerOverride(bool enabled, float valueWatts) {
    powerOverrideEnabled = enabled;
    powerOverrideValue_W = valueWatts;
}
