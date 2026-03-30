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
#include "serial_monitor.h"

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
    SERIAL_STEP("MAIN", "=== ECO Voice System Starting ===");
    SERIAL_STEP("MAIN", "Serial monitor active at %d baud", SERIAL_BAUD_RATE);
    SERIAL_STEP("MAIN", "Step 1/4: Initializing sensors");

    // Initialize Components — halt on sensor failure, warn on audio failure
    if (!sensors.init()) {
        SERIAL_ERROR("MAIN", "Sensor init failed. Check INA219 wiring.");
        while (1) { vTaskDelay(1000 / portTICK_PERIOD_MS); }  // halt
    }
    SERIAL_STEP("MAIN", "Step 2/4: Initializing appliance control");
    appliances.init();
    SERIAL_STEP("MAIN", "Step 3/4: Initializing audio handler");
    if (!audio.init()) {
        SERIAL_WARN("MAIN", "DFPlayer init failed. Check SD card and wiring.");
        SERIAL_WARN("MAIN", "System will run with Serial-only feedback.");
    }
    SERIAL_STEP("MAIN", "Step 4/4: Initializing voice recognition");
    if (!voiceRecog.init(&sensors)) {
        SERIAL_ERROR("MAIN", "Voice recognition init failed. Check ESP-SR model on SD card.");
        while (1) { vTaskDelay(1000 / portTICK_PERIOD_MS); }  // halt
    }

    SERIAL_STEP("MAIN", "System ready. Say '%s' to wake up", WAKE_WORD);
    audio.speak(TRACK_SYSTEM_READY);

    // Create main task — 32KB stack required for ESP-SR inference + FreeRTOS overhead
    xTaskCreate(main_task, "main_task", 32768, NULL, 5, NULL);
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
        SERIAL_STEP("VOICE", "Wake word '%s' detected. Moving to authentication state", WAKE_WORD);
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
        SERIAL_WARN("AUTH", "Authentication timeout. Returning to idle");
        audio.speak("Time expired. System locked.");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        return;
    }

    std::string code = voiceRecog.recognizeSecretCode();
    if (code.empty()) return;

    if (voiceRecog.verifySecretCode(code)) {
        SERIAL_STEP("AUTH", "Secret code accepted. System unlocked");
        audio.speak("Secret code correct. System unlocked.");

        currentState = STATE_UNLOCKED;
        lastCommandTime = esp_timer_get_time() / 1000;
        appliances.setStatusLED(true);

    } else {
        authAttempts++;
        SERIAL_WARN("AUTH", "Wrong code. Attempt %d/%d", authAttempts, MAX_AUTH_ATTEMPTS);

        if (authAttempts >= MAX_AUTH_ATTEMPTS) {
            // 3 failures reached — enforce 30-second penalty, then allow fresh attempts
            SERIAL_WARN("AUTH", "Max attempts reached. Enforcing %d-second lockout", AUTH_LOCKOUT_MS / 1000);
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
            SERIAL_STEP("AUTH", "Lockout over. Listening for secret code again");
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
        SERIAL_WARN("SYSTEM", "Auto-lock triggered due to inactivity");
        audio.speak("System locked successfully.");
        currentState = STATE_IDLE;
        appliances.setStatusLED(false);
        appliances.turnOffAll();
        return;
    }

    std::string command = voiceRecog.recognizeCommand();
    if (command.empty()) return;

    SERIAL_STEP("COMMAND", "Received command: %s", command.c_str());
    lastCommandTime = esp_timer_get_time() / 1000; // reset inactivity timer on every command

    if (command == "LOCK") {
        SERIAL_STEP("COMMAND", "Lock command received. Locking system");
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

    SERIAL_STEP("COMMAND", "Processing %s | motion=%s | lightLevel=%d",
                command.c_str(),
                motionDetected ? "YES" : "NO",
                lightLevel);

    if      (command == "LIGHT_ON")  handleLightOn(motionDetected, lightLevel);
    else if (command == "LIGHT_OFF") { appliances.setLight(false); audio.speak("Turning off the light."); }
    else if (command == "FAN_ON")    handleFanOn(motionDetected);
    else if (command == "FAN_OFF")   { appliances.setFan(false); audio.speak("Turning off the fan."); }
    else if (command == "STATUS")    reportStatus();
    else                             audio.speak("Sorry, I did not understand. Please repeat.");
}

static void handleLightOn(bool motionDetected, int lightLevel) {
    if (!motionDetected) {
        SERIAL_WARN("LIGHT", "No motion detected before light-on request");
        audio.speak("No motion detected. Do you still want to continue?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            SERIAL_STEP("LIGHT", "User declined light-on request");
            audio.speak("Light remains off.");
            return;
        }
        SERIAL_STEP("LIGHT", "User confirmed light-on without motion");
    }

    if (lightLevel > BRIGHTNESS_THRESHOLD) {
        SERIAL_WARN("LIGHT", "Ambient light already high (%d > %d)", lightLevel, BRIGHTNESS_THRESHOLD);
        audio.speak("It is already bright. Do you still want to turn on the light?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            SERIAL_STEP("LIGHT", "User declined light-on because room is already bright");
            audio.speak("Light remains off.");
            return;
        }
        SERIAL_STEP("LIGHT", "User confirmed light-on despite bright room");
    }

    // Voltage notifications only once we know the command will proceed
    if (sensors.isVoltageLow()) {
        SERIAL_WARN("LIGHT", "Low voltage detected before turning light on");
        audio.speak("Warning. Low voltage detected.");
    }
    if (sensors.isVoltageFluctuating()) {
        SERIAL_WARN("LIGHT", "Voltage fluctuation detected before turning light on");
        audio.speak("Warning. Voltage fluctuation detected.");
    }

    appliances.setLight(true);
    SERIAL_STEP("LIGHT", "Light relay turned ON");
    audio.speak("Light turned on successfully.");
}

static void handleFanOn(bool motionDetected) {
    if (!motionDetected) {
        SERIAL_WARN("FAN", "No motion detected before fan-on request");
        audio.speak("No motion detected. Do you still want to continue?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            SERIAL_STEP("FAN", "User declined fan-on request");
            audio.speak("Fan remains off.");
            return;
        }
        SERIAL_STEP("FAN", "User confirmed fan-on without motion");
    }

    // DHT11 gate — low temp or low humidity means fan is not needed
    float temp = sensors.getTemperature();
    float humidity = sensors.getHumidity();
    if (temp < TEMP_LOW_THRESHOLD || humidity < HUMIDITY_LOW_THRESHOLD) {
        SERIAL_WARN("FAN", "Temperature/humidity gate active | temp=%.1fC humidity=%.1f%%", temp, humidity);
        audio.speak("Temperature is low. Do you still want to turn on the fan?");
        if (voiceRecog.recognizeYesNo() != "YES") {
            SERIAL_STEP("FAN", "User declined fan-on because temperature/humidity is low");
            audio.speak("Fan remains off.");
            return;
        }
        SERIAL_STEP("FAN", "User confirmed fan-on despite temperature/humidity gate");
    }

    // Voltage notifications only once we know the command will proceed
    if (sensors.isVoltageLow()) {
        SERIAL_WARN("FAN", "Low voltage detected before turning fan on");
        audio.speak("Warning. Low voltage detected.");
    }
    if (sensors.isVoltageFluctuating()) {
        SERIAL_WARN("FAN", "Voltage fluctuation detected before turning fan on");
        audio.speak("Warning. Voltage fluctuation detected.");
    }

    appliances.setFan(true);
    SERIAL_STEP("FAN", "Fan relay turned ON");
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

    SERIAL_STEP("STATUS",
                "Motion=%s | LightLevel=%d | Temp=%.1fC | Humidity=%.1f%% | Current=%.2fA | Voltage=%.2fV | Light=%s | Fan=%s",
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
