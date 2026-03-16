/*
 * Sensor Handler Implementation
 */

#include "sensor_handler.h"
#include "config.h"

SensorHandler::SensorHandler() :
    motionDetected(false),
    lightLevel(0),
    current_mA(0),
    busVoltage(0),
    shuntVoltage(0),
    loadVoltage(0),
    power_mW(0),
    lastMotionTime(0) {

    ina219 = new Adafruit_INA219();
}

bool SensorHandler::init() {
    Serial.println("Initializing Sensors...");

    // Configure PIR sensor pin
    pinMode(PIR_PIN, INPUT);

    // Configure LDR pin (ADC)
    pinMode(LDR_PIN, INPUT);
    analogSetAttenuation(ADC_11db); // For 0-3.3V range

    // Initialize INA219 current sensor
    if (!ina219->begin()) {
        Serial.println("Failed to initialize INA219!");
        return false;
    }

    // Configure INA219
    // By default, INA219 is configured for 32V, 2A range
    // For higher current (up to 3.2A), use:
    ina219->setCalibration_32V_1A(); // Adjust based on your load

    Serial.println("All sensors initialized successfully");
    return true;
}

void SensorHandler::update() {
    readPIR();
    readLDR();
    readCurrentSensor();
}

void SensorHandler::readPIR() {
    int pirState = digitalRead(PIR_PIN);

    if (pirState == HIGH) {
        motionDetected = true;
        lastMotionTime = millis();
    } else {
        // Keep motion detected for MOTION_TIMEOUT_MS after last detection
        if (millis() - lastMotionTime > MOTION_TIMEOUT_MS) {
            motionDetected = false;
        }
    }
}

void SensorHandler::readLDR() {
    // Read analog value from LDR
    // Higher value = more light
    lightLevel = analogRead(LDR_PIN);

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

void SensorHandler::readCurrentSensor() {
    // Read values from INA219
    shuntVoltage = ina219->getShuntVoltage_mV();
    busVoltage = ina219->getBusVoltage_V();
    current_mA = ina219->getCurrent_mA();
    power_mW = ina219->getPower_mW();
    loadVoltage = busVoltage + (shuntVoltage / 1000.0);
}

bool SensorHandler::isMotionDetected() {
    return motionDetected;
}

int SensorHandler::getLightLevel() {
    return lightLevel;
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
