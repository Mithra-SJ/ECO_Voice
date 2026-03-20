/*
 * Sensor Handler Implementation
 */

#include "sensor_handler.h"
#include "config.h"

SensorHandler::SensorHandler() :
    dht(DHT11_PIN, DHT11),
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
    Serial.println("Initializing Sensors...");

    // Configure PIR sensor pin
    pinMode(PIR_PIN, INPUT);

    // Configure LDR pin (ADC)
    pinMode(LDR_PIN, INPUT);
    analogSetPinAttenuation(LDR_PIN, ADC_11db); // For 0-3.3V range on LDR pin only

    // Pre-fill LDR smoothing filter with real readings to avoid dark-bias on startup
    for (int i = 0; i < 10; i++) readLDR();

    // Initialize DHT11 temperature & humidity sensor
    dht.begin();
    delay(2000); // DHT11 needs 2s after power-on before first reliable read
    readDHT11();
    Serial.println("DHT11 initialized.");

    // Initialize INA219 current sensor
    if (!ina219.begin()) {
        Serial.println("Failed to initialize INA219!");
        return false;
    }

    ina219.setCalibration_32V_2A(); // 32V range, 2A max resolution
    readCurrentSensor();             // warmup read — sets loadVoltage baseline

    Serial.println("All sensors initialized successfully");
    return true;
}

void SensorHandler::update() {
    readPIR();
    readLDR();
    readDHT11();
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

void SensorHandler::readDHT11() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    // isnan() guards against failed reads — keep last valid value if read fails
    if (!isnan(t)) temperature = t;
    if (!isnan(h)) humidity    = h;
}

void SensorHandler::readCurrentSensor() {
    // Read values from INA219
    shuntVoltage = ina219.getShuntVoltage_mV();
    busVoltage   = ina219.getBusVoltage_V();
    current_mA   = ina219.getCurrent_mA();
    power_mW     = ina219.getPower_mW();

    float newLoad = busVoltage + (shuntVoltage / 1000.0);

    // Compute fluctuation delta vs previous reading
    // voltageInitialized prevents false spike on first read (0 → ~12V)
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
