/*
 * sensor_handler.cpp — ECO Voice Online Version
 */

#include "sensor_handler.h"
#include <Wire.h>

SensorHandler::SensorHandler()
    : ina219(0x40),
      dht(DHT11_PIN, DHT11),
      temperature(0.0f), humidity(0.0f),
      motionDetected(false), lightLevel(0),
      current(0.0f), voltage(0.0f), power(0.0f),
      lastVoltage(0.0f), voltageFluctuating(false),
      ina219Ready(false), lastMotionTime(0) {}

void SensorHandler::init() {
    // I2C for INA219
    Wire.begin(INA219_SDA, INA219_SCL);

    if (ina219.begin()) {
        ina219Ready = true;
        Serial.println("[SENSOR] INA219 ready at 0x40");
    } else {
        Serial.println("[SENSOR] INA219 not found — current readings disabled");
    }

    dht.begin();
    Serial.println("[SENSOR] DHT11 ready");

    pinMode(PIR_PIN, INPUT);
    Serial.println("[SENSOR] PIR ready");

    // LDR is analog input — no pinMode needed for analogRead
    Serial.println("[SENSOR] All sensors initialized");
}

void SensorHandler::update() {
    readPIR();
    readLDR();
    readDHT11();
    readINA219();
}

void SensorHandler::readPIR() {
    if (digitalRead(PIR_PIN) == HIGH) {
        motionDetected = true;
        lastMotionTime = millis();
    } else if ((millis() - lastMotionTime) > MOTION_TIMEOUT_MS) {
        motionDetected = false;
    }
}

void SensorHandler::readLDR() {
    lightLevel = analogRead(LDR_PIN);
}

void SensorHandler::readDHT11() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperature = t;
    if (!isnan(h)) humidity    = h;
}

void SensorHandler::readINA219() {
    if (!ina219Ready) return;

    float v = ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV() / 1000.0f);
    float c = ina219.getCurrent_mA() / 1000.0f;
    float p = v * c;

    voltageFluctuating = (lastVoltage > 0.5f) &&
                         (fabsf(v - lastVoltage) > VOLTAGE_FLUCTUATION_THRESHOLD);
    lastVoltage = v;
    voltage = v;
    current = c;
    power   = p;
}
