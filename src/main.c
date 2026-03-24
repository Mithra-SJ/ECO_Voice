#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
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
static SystemState currentState = STATE_IDLE;
static uint32_t authStartTime = 0;
static uint32_t lastCommandTime = 0;
static int authAttempts = 0;

// Hardware Instances
static AudioHandler audio;
static VoiceRecognition voiceRecog;
static SensorHandler sensors;
static ApplianceControl appliances;

// Forward declarations
static void handleIdleState();
static void handleAuthenticationState();
static void handleUnlockedState();
static void processCommand(const char* command);
static void handleLightOn(bool motionDetected, int lightLevel);
static void handleFanOn(bool motionDetected);
static void reportStatus();
static void main_task(void *pvParameters);

void app_main(void) {
    ESP_LOGI("MAIN", "=== ECO Voice System Starting ===\n");

    // Initialize Components — halt on sensor failure, warn on audio failure
    if (!sensors.init()) {
        ESP_LOGE("MAIN", "Sensor init failed. Check INA219 wiring.");
        while (1) { vTaskDelay(1000 / portTICK_PERIOD_MS); }  // halt
    }
    appliances.init();
    if (!audio.init()) {
        ESP_LOGW("MAIN", "DFPlayer init failed. Check SD card and wiring.");
        ESP_LOGW("MAIN", "System will run with Serial-only feedback.");
    }
    voiceRecog.init(&sensors);   // pass sensors

    ESP_LOGI("MAIN", "Ready. Say 'ECO' to wake up!\n");
    audio.speak("System ready.");

    // Create main task
    xTaskCreate(main_task, "main_task", 4096, NULL, 5, NULL);
}

static void main_task(void *pvParameters) {
    while (1) {
        // Update sensor readings every loop tick
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

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void handleIdleState() {
    if (voiceRecog.detectWakeWord()) {
        Serial.println("[VOICE] Wake word detected!");
        audio.speak("ECO activated. Listening for secret code.");

        currentState = STATE_WAITING_CODE;
        authStartTime = millis();
        authAttempts = 0;

        // Blink green LED to indicate listening
        appliances.blinkStatusLED(true, 3);
    }
}

static void handleAuthenticationState() {
    // Enforce 30-second window
    if ((millis() - authStartTime) > AUTH_TIMEOUT_MS) {
        Serial.println("[AUTH] Authentication timeout");
        audio.speak("Time expired. System locked.");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        return;
    }

    std::string code = voiceRecog.recognizeSecretCode();
    if (code.empty()) return;

    if (voiceRecog.verifySecretCode(code)) {
        Serial.println("[AUTH] Secret code correct - Unlocked!");
        audio.speak("Secret code correct. System unlocked.");

        currentState = STATE_UNLOCKED;
        lastCommandTime = millis();
        appliances.setStatusLED(true);

    } else {
        authAttempts++;
        Serial.print("[AUTH] Wrong code. Attempt ");
        Serial.print(authAttempts);
        Serial.print("/");
        Serial.println(MAX_AUTH_ATTEMPTS);

        if (authAttempts >= MAX_AUTH_ATTEMPTS) {
            Serial.println("[AUTH] Max attempts reached. Locking out.");
            audio.speak("System locked successfully.");
            currentState = STATE_IDLE;
            appliances.setStatusLED(false);
        } else {
            audio.speak("Wrong code. Try again.");
        }
    }
}

static void handleUnlockedState() {
    // Auto-lock on inactivity
    if ((millis() - lastCommandTime) > UNLOCK_TIMEOUT_MS) {
        Serial.println("[SYSTEM] Auto-lock: inactivity timeout");
        audio.speak("System locked successfully.");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        appliances.turnOffAll();
        return;
    }

    std::string command = voiceRecog.recognizeCommand();
    if (command.empty()) return;

    Serial.print("[COMMAND] Received: ");
    Serial.println(command.c_str());
    lastCommandTime = millis(); // reset inactivity timer on every command

    if (command == "LOCK") {
        Serial.println("[COMMAND] Locking system");
        audio.speak("System locked successfully.");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        appliances.turnOffAll();
    } else {
        processCommand(command);
    }
}

static void processCommand(const std::string& command) {
    bool motionDetected = sensors.isMotionDetected();
    int lightLevel = sensors.getLightLevel();

    // INA219 voltage notifications — speak before acting on any ON command
    if (command == "LIGHT_ON" || command == "FAN_ON") {
        if (sensors.isVoltageLow())         audio.speak("Warning. Low voltage detected.");
        if (sensors.isVoltageFluctuating()) audio.speak("Warning. Voltage fluctuation detected.");
    }

    if      (command == "LIGHT_ON")  handleLightOn(motionDetected, lightLevel);
    else if (command == "LIGHT_OFF") { appliances.setLight(false); audio.speak("Turning off the light."); }
    else if (command == "FAN_ON")    handleFanOn(motionDetected);
    else if (command == "FAN_OFF")   { appliances.setFan(false); audio.speak("Turning off the fan."); }
    else if (command == "STATUS")    reportStatus();
    else                             audio.speak("Sorry, I did not understand. Please repeat.");
}

static void handleLightOn(bool motionDetected, int lightLevel) {
    if (!motionDetected) {
        audio.speak("No motion detected. Do you still want to continue?");
        if (strcmp(voiceRecog.recognizeYesNo().c_str(), "YES") != 0) {
            audio.speak("Light remains off.");
            return;
        }
    }

    if (lightLevel > BRIGHTNESS_THRESHOLD) {
        audio.speak("It is already bright. Do you still want to turn on the light?");
        if (strcmp(voiceRecog.recognizeYesNo().c_str(), "YES") != 0) {
            audio.speak("Light remains off.");
            return;
        }
    }

    appliances.setLight(true);
    audio.speak("Light turned on successfully.");
}

static void handleFanOn(bool motionDetected) {
    if (!motionDetected) {
        audio.speak("No motion detected. Do you still want to continue?");
        if (strcmp(voiceRecog.recognizeYesNo().c_str(), "YES") != 0) {
            audio.speak("Fan remains off.");
            return;
        }
    }

    // DHT11 gate — low temp or low humidity means fan is not needed
    float temp = sensors.getTemperature();
    float humidity = sensors.getHumidity();
    if (temp < TEMP_LOW_THRESHOLD || humidity < HUMIDITY_LOW_THRESHOLD) {
        audio.speak("Temperature is low. Do you still want to turn on the fan?");
        if (strcmp(voiceRecog.recognizeYesNo().c_str(), "YES") != 0) {
            audio.speak("Fan remains off.");
            return;
        }
    }

    appliances.setFan(true);
    audio.speak("Fan turned on successfully.");
}

static void reportStatus() {
    bool motion = sensors.isMotionDetected();
    int light = sensors.getLightLevel();
    float temp = sensors.getTemperature();
    float humidity = sensors.getHumidity();
    float current = sensors.getCurrent();
    float voltage = sensors.getVoltage();
    bool lightOn = appliances.isLightOn();
    bool fanOn = appliances.isFanOn();

    char statusMsg[250];
    snprintf(statusMsg, sizeof(statusMsg),
             "Motion: %s. Light: %d. Temp: %.1fC. Humidity: %.1f%%. Current: %.2fA. Voltage: %.2fV. Light: %s. Fan: %s.\n",
             motion ? "detected" : "not detected",
             light, temp, humidity, current, voltage,
             lightOn ? "on" : "off",
             fanOn ? "on" : "off");
    Serial.println(statusMsg);

    audio.speak("Here is the system status.");
    audio.speak(motion ? "Motion detected." : "No motion detected.");
    audio.speak(lightOn ? "Light is currently on." : "Light is currently off.");
    audio.speak(fanOn ? "Fan is currently on." : "Fan is currently off.");
}
