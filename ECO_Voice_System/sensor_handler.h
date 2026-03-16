/*
 * Sensor Handler - Manages all environmental sensors
 * PIR Motion, LDR Light, INA219 Current/Voltage
 */

#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>
#include <Adafruit_INA219.h>

class SensorHandler {
public:
    SensorHandler();
    bool init();
    void update();

    // Sensor Readings
    bool isMotionDetected();
    int getLightLevel();          // 0-4095 ADC value
    float getCurrent();           // Amps
    float getVoltage();           // Volts
    float getPower();             // Watts

private:
    Adafruit_INA219 *ina219;
    bool motionDetected;
    int lightLevel;
    float current_mA;
    float busVoltage;
    float shuntVoltage;
    float loadVoltage;
    float power_mW;

    unsigned long lastMotionTime;
    void readPIR();
    void readLDR();
    void readCurrentSensor();
};

#endif // SENSOR_HANDLER_H
