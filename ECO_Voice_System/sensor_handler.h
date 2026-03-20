/*
 * Sensor Handler - Manages all environmental sensors
 * PIR Motion, LDR Light, INA219 Current/Voltage
 */

#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>
#include <Adafruit_INA219.h>
#include <DHT.h>

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

private:
    Adafruit_INA219 ina219;
    DHT dht;

    bool motionDetected;
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

    unsigned long lastMotionTime;
    void readPIR();
    void readLDR();
    void readDHT11();
    void readCurrentSensor();
};

#endif // SENSOR_HANDLER_H
