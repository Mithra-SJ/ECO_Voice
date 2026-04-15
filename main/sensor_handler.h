/*
 * Sensor Handler - Manages all environmental sensors
 * PIR Motion, LDR Light, DHT11 Temp/Humidity (INA219 simplified)
 */

#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "dht11.h"  // ESP-IDF DHT11 library

#ifdef __cplusplus
}
#endif

class SensorHandler {
public:
    SensorHandler();
    bool init();
    void update();

    // Sensor Readings
    bool isMotionDetected();
    int getLightLevel();          // 0-4095 ADC value
    float getTemperature();       // °C
    float getHumidity();          // %RH
    float getCurrent();           // Amps
    float getVoltage();           // Volts
    float getPower();             // Watts

    // Voltage status (INA219 monitoring)
    bool isVoltageLow();          // true if below LOW_VOLTAGE_THRESHOLD
    bool isVoltageFluctuating();  // true if delta between readings exceeds threshold
    bool isCurrentFluctuating();  // true if current delta between readings exceeds threshold
    bool isOvercurrent();         // true if current exceeds OVERCURRENT_THRESHOLD
    bool isIna219Available() const;
    bool isDht11Available() const;

    // Test hooks
    void clearOverrides();
    void setMotionOverride(bool enabled, bool value);
    void setLightLevelOverride(bool enabled, int value);
    void setTemperatureOverride(bool enabled, float value);
    void setHumidityOverride(bool enabled, float value);
    void setVoltageOverride(bool enabled, float value);
    void setCurrentOverride(bool enabled, float valueAmps);
    void setPowerOverride(bool enabled, float valueWatts);

private:
    bool motionDetected;
    bool ina219Available;
    bool dht11Available;
    int lightLevel;
    float temperature;
    float humidity;
    float current_mA;
    float busVoltage;
    float shuntVoltage;
    float loadVoltage;
    float power_mW;
    float voltageDelta;
    bool voltageInitialized;
    float lastCurrent_A;
    float currentDelta_A;
    bool currentInitialized;
    int dht11FailureCount;
    int64_t lastDht11ReadMs;
    int64_t lastDht11LogMs;

    bool motionOverrideEnabled;
    bool motionOverrideValue;
    bool lightOverrideEnabled;
    int lightOverrideValue;
    bool temperatureOverrideEnabled;
    float temperatureOverrideValue;
    bool humidityOverrideEnabled;
    float humidityOverrideValue;
    bool voltageOverrideEnabled;
    float voltageOverrideValue;
    bool currentOverrideEnabled;
    float currentOverrideValue_A;
    bool powerOverrideEnabled;
    float powerOverrideValue_W;

    unsigned long lastMotionTime;
    void readPIR();
    void readLDR();
    void readDHT11();
    void readCurrentSensor();
};

#endif // SENSOR_HANDLER_H
