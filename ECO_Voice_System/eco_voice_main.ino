/*
 * ECO Voice - Offline Voice Recognition Home Automation
 * ESP32-S3 with Edge AI Voice Control
 *
 * Features:
 * - Offline wake word detection (energy VAD + ESP-SR production path)
 * - Secret code authentication with brute-force lockout
 * - Context-aware sensor integration (PIR, LDR, INA219)
 * - DFPlayer Mini voice feedback
 * - Auto-lock on inactivity
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "audio_handler.h"
#include "voice_recognition.h"
#include "sensor_handler.h"
#include "appliance_control.h"

// System States
typedef enum {
    STATE_IDLE,
    STATE_WAITING_CODE,
    STATE_UNLOCKED
} SystemState;

// Global Variables
SystemState currentState    = STATE_IDLE;
unsigned long authStartTime = 0;
unsigned long lastCommandTime = 0;
int authAttempts            = 0;

// Hardware Instances
AudioHandler audio;
VoiceRecognition voiceRecog;
SensorHandler sensors;
ApplianceControl appliances;

// Forward declarations
void handleIdleState();
void handleAuthenticationState();
void handleUnlockedState();
void processCommand(const String& command);
void handleLightOn(bool motionDetected, int lightLevel);
void handleFanOn(bool motionDetected, float current);
void reportStatus();

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println("ECO Voice System Starting...");

    // Initialize I2C for INA219
    Wire.begin(INA219_SDA, INA219_SCL);

    // Initialize Components — halt on sensor failure, warn on audio failure
    if (!sensors.init()) {
        Serial.println("FATAL: Sensor init failed. Check INA219 wiring.");
        while (1) { delay(1000); }  // halt — sensor data is required for safe operation
    }
    appliances.init();
    if (!audio.init()) {
        Serial.println("WARNING: DFPlayer init failed. Check SD card and wiring.");
        Serial.println("System will run with Serial-only feedback.");
    }
    voiceRecog.init(&sensors);   // pass sensors so recognition loops stay fresh

    Serial.println("System Ready. Say 'ECO' to wake up!");
}

void loop() {
    // Update sensor readings every tick
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
    }

    delay(10);
}

void handleIdleState() {
    if (voiceRecog.detectWakeWord()) {
        Serial.println("Wake word detected!");
        audio.speak("Listening for secret code");

        currentState  = STATE_WAITING_CODE;
        authStartTime = millis();
        authAttempts  = 0;

        // Blink green LED to indicate listening
        appliances.blinkStatusLED(true, 3);
    }
}

void handleAuthenticationState() {
    // Enforce 30-second window
    if (millis() - authStartTime > AUTH_TIMEOUT_MS) {
        Serial.println("Authentication timeout");
        audio.speak("Authentication timeout");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        return;
    }

    String code = voiceRecog.recognizeSecretCode();
    if (code.length() == 0) return;

    if (voiceRecog.verifySecretCode(code)) {
        Serial.println("Secret code correct - Unlocked!");
        audio.speak("Unlocked. Ready for commands");

        currentState    = STATE_UNLOCKED;
        lastCommandTime = millis();
        appliances.setStatusLED(true);

    } else {
        authAttempts++;
        Serial.print("Wrong code. Attempt ");
        Serial.print(authAttempts);
        Serial.print("/");
        Serial.println(MAX_AUTH_ATTEMPTS);

        if (authAttempts >= MAX_AUTH_ATTEMPTS) {
            Serial.println("Max attempts reached. Locking out.");
            audio.speak("System locked");
            currentState = STATE_IDLE;
            appliances.setStatusLED(false);
        } else {
            audio.speak("Wrong code. Try again");
        }
    }
}

void handleUnlockedState() {
    // Auto-lock on inactivity
    if (millis() - lastCommandTime > UNLOCK_TIMEOUT_MS) {
        Serial.println("Auto-lock: inactivity timeout");
        audio.speak("System locked");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        appliances.turnOffAll();
        return;
    }

    String command = voiceRecog.recognizeCommand();
    if (command.length() == 0) return;

    Serial.print("Command received: ");
    Serial.println(command);
    lastCommandTime = millis(); // reset inactivity timer on every command

    if (command == "LOCK") {
        Serial.println("Locking system");
        audio.speak("System locked");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        appliances.turnOffAll();
    } else {
        processCommand(command);
    }
}

void processCommand(const String& command) {
    bool motionDetected = sensors.isMotionDetected();
    int lightLevel      = sensors.getLightLevel();
    float current       = sensors.getCurrent();

    if      (command == "LIGHT_ON")  handleLightOn(motionDetected, lightLevel);
    else if (command == "LIGHT_OFF") { appliances.setLight(false); audio.speak("Light turned off"); }
    else if (command == "FAN_ON")    handleFanOn(motionDetected, current);
    else if (command == "FAN_OFF")   { appliances.setFan(false); audio.speak("Fan turned off"); }
    else if (command == "STATUS")    reportStatus();
    else                             audio.speak("Command not recognized");
}

void handleLightOn(bool motionDetected, int lightLevel) {
    if (!motionDetected) {
        audio.speak("No motion detected. Do you still want to continue?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            audio.speak("Light not turned on");
            return;
        }
    }

    if (lightLevel > BRIGHTNESS_THRESHOLD) {
        audio.speak("It's bright already. Do you still want to switch on light?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            audio.speak("Light not turned on");
            return;
        }
    }

    appliances.setLight(true);
    audio.speak("Light turned on");
}

void handleFanOn(bool motionDetected, float current) {
    if (!motionDetected) {
        audio.speak("No motion detected. Do you still want to continue?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            audio.speak("Fan not turned on");
            return;
        }
    }

    if (current > CURRENT_THRESHOLD) {
        audio.speak("High current detected. Do you still want to turn on fan?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            audio.speak("Fan not turned on for safety");
            return;
        }
    }

    appliances.setFan(true);
    audio.speak("Fan turned on");
}

void reportStatus() {
    bool motion  = sensors.isMotionDetected();
    int light    = sensors.getLightLevel();
    float current = sensors.getCurrent();
    float voltage = sensors.getVoltage();
    bool lightOn = appliances.isLightOn();
    bool fanOn   = appliances.isFanOn();

    char statusMsg[200];
    snprintf(statusMsg, sizeof(statusMsg),
             "Motion: %s. Light level: %d. Current: %.2fA. Voltage: %.2fV. Light: %s. Fan: %s.",
             motion   ? "detected" : "not detected",
             light, current, voltage,
             lightOn ? "on" : "off",
             fanOn   ? "on" : "off");
    Serial.println(statusMsg);

    audio.speak(motion  ? "Motion detected." : "No motion.");
    audio.speak(lightOn ? "Light is on."     : "Light is off.");
    audio.speak(fanOn   ? "Fan is on."       : "Fan is off.");
}
