#include <stdio.h>
#include <string>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
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
static void processCommand(const std::string& command);
static void handleLightOn(bool motionDetected, int lightLevel);
static void handleFanOn(bool motionDetected);
static void reportStatus();
static void main_task(void *pvParameters);

extern "C" void app_main(void) {
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
    if (!voiceRecog.init(&sensors)) {
        ESP_LOGE("MAIN", "Voice recognition init failed. Check ESP-SR model on SD card.");
        while (1) { vTaskDelay(1000 / portTICK_PERIOD_MS); }  // halt
    }

    ESP_LOGI("MAIN", "Ready. Say 'hi esp' to wake up!\n");
    audio.speak(TRACK_SYSTEM_READY);

    // Create main task
    xTaskCreate(main_task, "main_task", 8192, NULL, 5, NULL);
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
        ESP_LOGI("VOICE", "Wake word 'hi esp' detected!");
        audio.speak(TRACK_LISTENING_CODE);

        currentState = STATE_WAITING_CODE;
        authStartTime = esp_timer_get_time() / 1000;
        authAttempts = 0;

        // Blink green LED to indicate listening
        appliances.blinkStatusLED(true, 3);
    }
}

static void handleAuthenticationState() {
    // Enforce 30-second window
    if ((esp_timer_get_time() / 1000 - authStartTime) > AUTH_TIMEOUT_MS) {
        ESP_LOGI("AUTH", "Authentication timeout");
        audio.speak("Time expired. System locked.");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        return;
    }

    std::string code = voiceRecog.recognizeSecretCode();
    if (code.empty()) return;

    if (voiceRecog.verifySecretCode(code)) {
        ESP_LOGI("AUTH", "Secret code correct - Unlocked!");
        audio.speak("Secret code correct. System unlocked.");

        currentState = STATE_UNLOCKED;
        lastCommandTime = esp_timer_get_time() / 1000;
        appliances.setStatusLED(true);

    } else {
        authAttempts++;
        ESP_LOGI("AUTH", "Wrong code. Attempt %d/%d", authAttempts, MAX_AUTH_ATTEMPTS);

        if (authAttempts >= MAX_AUTH_ATTEMPTS) {
            // 3 failures reached — enforce 30-second penalty, then allow fresh attempts
            ESP_LOGI("AUTH", "Max attempts reached. Enforcing %d-second lockout.", AUTH_LOCKOUT_MS / 1000);
            audio.speak("Too many wrong attempts. Please wait 30 seconds.");
            appliances.setStatusLED(false);       // red LED during lockout
            appliances.blinkStatusLED(false, 3);  // blink red to signal lockout
            // Wait AUTH_LOCKOUT_MS while keeping sensors live
            uint32_t lockStart = esp_timer_get_time() / 1000;
            while ((esp_timer_get_time() / 1000 - lockStart) < AUTH_LOCKOUT_MS) {
                sensors.update();
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }

            // Reset for a fresh round — user stays in STATE_WAITING_CODE
            authAttempts  = 0;
            authStartTime = esp_timer_get_time() / 1000;
            ESP_LOGI("AUTH", "Lockout over. Accepting attempts again.");
            audio.speak("Listening for secret code.");
            appliances.blinkStatusLED(true, 2);
        } else {
            audio.speak("Wrong code. Try again.");
        }
    }
}

static void handleUnlockedState() {
    // Auto-lock on inactivity
    if ((esp_timer_get_time() / 1000 - lastCommandTime) > UNLOCK_TIMEOUT_MS) {
        ESP_LOGI("SYSTEM", "Auto-lock: inactivity timeout");
        audio.speak("System locked successfully.");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        appliances.turnOffAll();
        return;
    }

    std::string command = voiceRecog.recognizeCommand();
    if (command.empty()) return;

    ESP_LOGI("COMMAND", "Received: %s", command.c_str());
    lastCommandTime = esp_timer_get_time() / 1000; // reset inactivity timer on every command

    if (command == "LOCK") {
        ESP_LOGI("COMMAND", "Locking system");
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
        if (voiceRecog.recognizeYesNo() != "YES") {
            audio.speak("Light remains off.");
            return;
        }
    }

    if (lightLevel > BRIGHTNESS_THRESHOLD) {
        audio.speak("It is already bright. Do you still want to turn on the light?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            audio.speak("Light remains off.");
            return;
        }
    }

    // Voltage notifications only once we know the command will proceed
    if (sensors.isVoltageLow())         audio.speak("Warning. Low voltage detected.");
    if (sensors.isVoltageFluctuating()) audio.speak("Warning. Voltage fluctuation detected.");

    appliances.setLight(true);
    audio.speak("Light turned on successfully.");
}

static void handleFanOn(bool motionDetected) {
    if (!motionDetected) {
        audio.speak("No motion detected. Do you still want to continue?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            audio.speak("Fan remains off.");
            return;
        }
    }

    // DHT11 gate — low temp or low humidity means fan is not needed
    float temp = sensors.getTemperature();
    float humidity = sensors.getHumidity();
    if (temp < TEMP_LOW_THRESHOLD || humidity < HUMIDITY_LOW_THRESHOLD) {
        audio.speak("Temperature is low. Do you still want to turn on the fan?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            audio.speak("Fan remains off.");
            return;
        }
    }

    // Voltage notifications only once we know the command will proceed
    if (sensors.isVoltageLow())         audio.speak("Warning. Low voltage detected.");
    if (sensors.isVoltageFluctuating()) audio.speak("Warning. Voltage fluctuation detected.");

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

    ESP_LOGI("STATUS", "Motion: %s. Light: %d. Temp: %.1fC. Humidity: %.1f%%. Current: %.2fA. Voltage: %.2fV. Light: %s. Fan: %s.",
             motion ? "detected" : "not detected",
             light, temp, humidity, current, voltage,
             lightOn ? "on" : "off",
             fanOn ? "on" : "off");

    char statusMessage[256];
    snprintf(statusMessage, sizeof(statusMessage),
             "Status report: motion %s, light level %d, temperature %.1f C, humidity %.1f percent, current %.2f A, voltage %.2f V, light %s, fan %s.",
             motion ? "detected" : "not detected",
             light, temp, humidity, current, voltage,
             lightOn ? "on" : "off",
             fanOn ? "on" : "off");
    audio.speak(statusMessage);

    if (temp < TEMP_LOW_THRESHOLD || humidity < HUMIDITY_LOW_THRESHOLD) {
        audio.speak("Temperature or humidity is low.");
    }

    if (sensors.isVoltageLow())              audio.speak("Warning. Low voltage detected.");
    else if (sensors.isVoltageFluctuating()) audio.speak("Warning. Voltage fluctuation detected.");
    else                                     audio.speak("Current and voltage are normal.");
}
