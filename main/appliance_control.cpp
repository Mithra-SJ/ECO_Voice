/*
 * appliance_control.cpp — ECO Voice Online Version
 */

#include "appliance_control.h"

#if RELAY_ACTIVE_LOW
static constexpr int RELAY_ON  = LOW;
static constexpr int RELAY_OFF = HIGH;
#else
static constexpr int RELAY_ON  = HIGH;
static constexpr int RELAY_OFF = LOW;
#endif

ApplianceControl::ApplianceControl() : lightOn(false), fanOn(false) {}

void ApplianceControl::init() {
    pinMode(RELAY_LIGHT_PIN, OUTPUT);
    pinMode(RELAY_FAN_PIN,   OUTPUT);
    pinMode(LED_GREEN_PIN,   OUTPUT);
    pinMode(LED_RED_PIN,     OUTPUT);

    // Start with everything off
    writeRelay(RELAY_LIGHT_PIN, false);
    writeRelay(RELAY_FAN_PIN,   false);
    setOnlineLED(false);  // red on until Firebase connects

    Serial.println("[APPLIANCE] Initialized — all off");
}

void ApplianceControl::setLight(bool on) {
    lightOn = on;
    writeRelay(RELAY_LIGHT_PIN, on);
    Serial.printf("[APPLIANCE] Light -> %s\n", on ? "ON" : "OFF");
}

void ApplianceControl::setFan(bool on) {
    fanOn = on;
    writeRelay(RELAY_FAN_PIN, on);
    Serial.printf("[APPLIANCE] Fan -> %s\n", on ? "ON" : "OFF");
}

void ApplianceControl::turnOffAll() {
    setLight(false);
    setFan(false);
}

void ApplianceControl::setOnlineLED(bool online) {
    digitalWrite(LED_GREEN_PIN, online ? HIGH : LOW);
    digitalWrite(LED_RED_PIN,   online ? LOW  : HIGH);
}

void ApplianceControl::writeRelay(int pin, bool on) {
    digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
}
