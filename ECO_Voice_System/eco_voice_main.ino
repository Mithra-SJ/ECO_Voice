/*
 * ECO Voice - Offline Voice Recognition Home Automation
 * ESP32-S3 with Edge AI Voice Control
 *
 * Features:
 * - Offline wake word detection
 * - Secret code authentication
 * - Context-aware sensor integration
 * - Voice feedback
 */

#include <Arduino.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "config.h"
#include "audio_handler.h"
#include "voice_recognition.h"
#include "sensor_handler.h"
#include "appliance_control.h"

// System States
typedef enum {
    STATE_IDLE,
    STATE_WAITING_CODE,
    STATE_UNLOCKED,
    STATE_PROCESSING_COMMAND
} SystemState;

// Global Variables
SystemState currentState = STATE_IDLE;
unsigned long authStartTime = 0;
bool isAuthenticated = false;

// Hardware Instances
Adafruit_INA219 ina219;
AudioHandler audio;
VoiceRecognition voiceRecog;
SensorHandler sensors;
ApplianceControl appliances;

void setup() {
    Serial.begin(115200);
    Serial.println("ECO Voice System Starting...");

    // Initialize I2C for INA219
    Wire.begin(INA219_SDA, INA219_SCL);

    // Initialize Hardware
    if (!ina219.begin()) {
        Serial.println("Failed to find INA219 chip");
    }

    // Initialize Components
    sensors.init();
    appliances.init();
    audio.init();
    voiceRecog.init();

    // Set LED status: Red = locked
    appliances.setStatusLED(false);

    Serial.println("System Ready. Say 'ECO' to wake up!");
}

void loop() {
    // Update sensor readings
    sensors.update();

    switch (currentState) {
        case STATE_IDLE:
            handleIdleState();
            break;

        case STATE_WAITING_CODE:
            handleAuthenticationState();
            break;

        case STATE_UNLOCKED:
            handleUnlockedState();
            break;

        case STATE_PROCESSING_COMMAND:
            handleCommandProcessing();
            break;
    }

    delay(10);
}

void handleIdleState() {
    // Listen for wake word "ECO"
    if (voiceRecog.detectWakeWord()) {
        Serial.println("Wake word detected!");
        audio.speak("Listening for secret code");

        currentState = STATE_WAITING_CODE;
        authStartTime = millis();
        isAuthenticated = false;

        // Blink green LED to indicate listening
        appliances.blinkStatusLED(true, 3);
    }
}

void handleAuthenticationState() {
    // Check timeout (30 seconds)
    if (millis() - authStartTime > AUTH_TIMEOUT_MS) {
        Serial.println("Authentication timeout");
        audio.speak("Authentication timeout");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false); // Red LED
        return;
    }

    // Listen for secret code
    String code = voiceRecog.recognizeSecretCode();

    if (code.length() > 0) {
        if (voiceRecog.verifySecretCode(code)) {
            Serial.println("Secret code correct - Unlocked!");
            audio.speak("Unlocked. Ready for commands");

            isAuthenticated = true;
            currentState = STATE_UNLOCKED;
            appliances.setStatusLED(true); // Green LED
        } else {
            Serial.println("Wrong secret code");
            audio.speak("Wrong code. Try again");
            // Allow retry within 30 seconds
        }
    }
}

void handleUnlockedState() {
    // Listen for commands or lock command
    String command = voiceRecog.recognizeCommand();

    if (command.length() > 0) {
        Serial.print("Command received: ");
        Serial.println(command);

        if (command == "LOCK") {
            Serial.println("Locking system");
            audio.speak("System locked");

            isAuthenticated = false;
            currentState = STATE_IDLE;
            appliances.setStatusLED(false); // Red LED
            appliances.turnOffAll(); // Optional: turn off all appliances
        } else {
            currentState = STATE_PROCESSING_COMMAND;
            processCommand(command);
        }
    }
}

void handleCommandProcessing() {
    // This state is brief - just processes and returns to unlocked
    currentState = STATE_UNLOCKED;
}

void processCommand(String command) {
    // Read current sensor values
    bool motionDetected = sensors.isMotionDetected();
    int lightLevel = sensors.getLightLevel();
    float current = sensors.getCurrent();

    if (command == "LIGHT_ON") {
        handleLightOn(motionDetected, lightLevel);
    }
    else if (command == "LIGHT_OFF") {
        appliances.setLight(false);
        audio.speak("Light turned off");
    }
    else if (command == "FAN_ON") {
        handleFanOn(motionDetected, current);
    }
    else if (command == "FAN_OFF") {
        appliances.setFan(false);
        audio.speak("Fan turned off");
    }
    else if (command == "STATUS") {
        reportStatus();
    }
    else {
        audio.speak("Command not recognized");
    }
}

void handleLightOn(bool motionDetected, int lightLevel) {
    // Check motion first
    if (!motionDetected) {
        audio.speak("No motion detected. Do you still want to continue?");
        String response = voiceRecog.recognizeYesNo();

        if (response != "YES") {
            audio.speak("Light not turned on");
            return;
        }
    }

    // Check brightness
    if (lightLevel > BRIGHTNESS_THRESHOLD) {
        audio.speak("It's bright already. Do you still want to switch on light?");
        String response = voiceRecog.recognizeYesNo();

        if (response != "YES") {
            audio.speak("Light not turned on");
            return;
        }
    }

    // Turn on light
    appliances.setLight(true);
    audio.speak("Light turned on");
}

void handleFanOn(bool motionDetected, float current) {
    // Check motion first
    if (!motionDetected) {
        audio.speak("No motion detected. Do you still want to continue?");
        String response = voiceRecog.recognizeYesNo();

        if (response != "YES") {
            audio.speak("Fan not turned on");
            return;
        }
    }

    // Check current load
    if (current > CURRENT_THRESHOLD) {
        audio.speak("High current detected. Do you still want to turn on fan?");
        String response = voiceRecog.recognizeYesNo();

        if (response != "YES") {
            audio.speak("Fan not turned on for safety");
            return;
        }
    }

    // Turn on fan
    appliances.setFan(true);
    audio.speak("Fan turned on");
}

void reportStatus() {
    bool motion = sensors.isMotionDetected();
    int light = sensors.getLightLevel();
    float current = sensors.getCurrent();
    float voltage = sensors.getVoltage();
    bool lightOn = appliances.isLightOn();
    bool fanOn = appliances.isFanOn();

    char statusMsg[200];
    snprintf(statusMsg, sizeof(statusMsg),
             "Motion: %s. Light level: %d. Current: %.2f amps. Voltage: %.2f volts. Light is %s. Fan is %s.",
             motion ? "detected" : "not detected",
             light,
             current,
             voltage,
             lightOn ? "on" : "off",
             fanOn ? "on" : "off");

    Serial.println(statusMsg);

    // Simplified audio feedback
    if (motion) {
        audio.speak("Motion detected.");
    } else {
        audio.speak("No motion.");
    }

    if (lightOn) {
        audio.speak("Light is on.");
    } else {
        audio.speak("Light is off.");
    }

    if (fanOn) {
        audio.speak("Fan is on.");
    } else {
        audio.speak("Fan is off.");
    }
}
