/*
 * Appliance Control Implementation
 */

#include "appliance_control.h"
#include "config.h"

ApplianceControl::ApplianceControl() :
    lightState(false),
    fanState(false),
    unlockedState(false) {
}

void ApplianceControl::init() {
    Serial.println("Initializing Appliance Control...");

    // Configure relay pins
    pinMode(RELAY_LIGHT_PIN, OUTPUT);
    pinMode(RELAY_FAN_PIN, OUTPUT);

    // Configure LED pins
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);

    // Initialize relays off
    digitalWrite(RELAY_LIGHT_PIN, LOW);
    digitalWrite(RELAY_FAN_PIN, LOW);

    // Set initial LED state through the single authoritative path
    setStatusLED(false); // locked = red LED ON

    Serial.println("Appliance Control Ready");
}

void ApplianceControl::setLight(bool state) {
    lightState = state;
    updateRelay(RELAY_LIGHT_PIN, state);

    Serial.print("Light turned ");
    Serial.println(state ? "ON" : "OFF");
}

void ApplianceControl::setFan(bool state) {
    fanState = state;
    updateRelay(RELAY_FAN_PIN, state);

    Serial.print("Fan turned ");
    Serial.println(state ? "ON" : "OFF");
}

void ApplianceControl::turnOffAll() {
    setLight(false);
    setFan(false);
    Serial.println("All appliances turned OFF");
}

bool ApplianceControl::isLightOn() {
    return lightState;
}

bool ApplianceControl::isFanOn() {
    return fanState;
}

void ApplianceControl::updateRelay(int pin, bool state) {
    // Relay is active HIGH per spec (High=ON, Low=OFF)
    digitalWrite(pin, state ? HIGH : LOW);
}

void ApplianceControl::setStatusLED(bool unlocked) {
    unlockedState = unlocked;
    if (unlocked) {
        // System unlocked - Green LED ON, Red LED OFF
        digitalWrite(LED_GREEN_PIN, HIGH);
        digitalWrite(LED_RED_PIN, LOW);
        Serial.println("Status: UNLOCKED (Green LED)");
    } else {
        // System locked - Red LED ON, Green LED OFF
        digitalWrite(LED_GREEN_PIN, LOW);
        digitalWrite(LED_RED_PIN, HIGH);
        Serial.println("Status: LOCKED (Red LED)");
    }
}

void ApplianceControl::blinkStatusLED(bool green, int times) {
    int ledPin = green ? LED_GREEN_PIN : LED_RED_PIN;

    for (int i = 0; i < times; i++) {
        digitalWrite(ledPin, HIGH);
        delay(200);
        digitalWrite(ledPin, LOW);
        delay(200);
    }

    // Restore LED to the correct lock/unlock state
    setStatusLED(unlockedState);
}
