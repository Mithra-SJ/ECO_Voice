/*
 * sensor_handler.h — ECO Voice Online Version
 * Reads PIR, LDR, DHT11, INA219
 */

#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>
#include <Adafruit_INA219.h>
#include <DHT.h>
#include "config.h"

class SensorHandler {
public:
    SensorHandler();
    void init();
    void update();

    float getTemperature() const  { return temperature; }
    float getHumidity() const     { return humidity; }
    bool isMotionDetected() const { return motionDetected; }
    int getLightLevel() const     { return lightLevel; }
    float getCurrent() const      { return current; }
    float getVoltage() const      { return voltage; }
    float getPower() const        { return power; }
    bool isVoltageLow() const     { return voltage > 0.5f && voltage < LOW_VOLTAGE_THRESHOLD; }
    bool isVoltageFluctuating() const { return voltageFluctuating; }

private:
    Adafruit_INA219 ina219;
    DHT dht;

    float temperature;
    float humidity;
    bool motionDetected;
    int lightLevel;
    float current;
    float voltage;
    float power;
    float lastVoltage;
    bool voltageFluctuating;
    bool ina219Ready;
    unsigned long lastMotionTime;

    void readPIR();
    void readLDR();
    void readDHT11();
    void readINA219();
};

#endif // SENSOR_HANDLER_H
