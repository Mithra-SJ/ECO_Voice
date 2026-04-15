#include <stdio.h>
#include <string>
#include <deque>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "config.h"
#include "sensor_handler.h"
#include "appliance_control.h"
#include "audio_handler.h"
#include "voice_recognition.h"
#include "secrets.h"

static SensorHandler sensors;
static ApplianceControl appliances;
static AudioHandler audio;
static VoiceRecognition microphone;

struct MicLogEntry {
    int64_t timestampMs;
    std::string phrase;
};

enum class PendingActionType {
    None,
    LightOn,
    FanOn,
};

struct PendingConfirmation {
    PendingActionType action;
    std::string prompt;
    AudioTrack advisoryTrack;
};

static std::deque<MicLogEntry> micLog;
static portMUX_TYPE micLogMux = portMUX_INITIALIZER_UNLOCKED;
static constexpr int64_t MIC_LOG_RETENTION_MS = 2 * 60 * 1000;
static volatile bool liveMicLogEnabled = false;
static PendingConfirmation pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };

static void sensor_task(void *pvParameters);
static void serial_task(void *pvParameters);
static void microphone_task(void *pvParameters);
static void printHelp();
static void printPrompt();
static void printStatus();
static void processCommand(const std::string& command);
static void handleLightOn();
static void handleLightOff();
static void handleFanOn();
static void handleFanOff();
static void handleAllOff();
static void performLightOn();
static void performFanOn();
static bool commandRequiresUnlock(const std::string& command);
static std::string normalizeCommand(const char *input);
static void addMicLogEntry(const MicLogEntry& entry);
static void printMicLogHistory();
static void setLiveMicLog(bool enabled);
static bool tryUnlockWithSecret(const std::string& command);
static std::string mapVoicePhraseToCommand(const std::string& phrase);
static void printVoiceHelp();
static void startConfirmation(PendingActionType action, const char *prompt, AudioTrack advisoryTrack);
static void handleConfirmationResponse(bool accepted);
static bool handleSensorTestCommand(const std::string& command);
static void logSensorAlerts();
static void reportPowerWarnings();

extern "C" void app_main(void) {
    ESP_LOGI("MAIN", "=== ECO Serial Monitor Starting ===");

    if (!sensors.init()) {
        ESP_LOGE("MAIN", "Sensor init failed. Check INA219 wiring.");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    appliances.init();
    appliances.setActivityLED(false);

    const bool audioReady = audio.init();
    if (!audioReady) {
        ESP_LOGW("MAIN", "DFPlayer init failed. Audio prompts are disabled.");
    }

    if (!microphone.init(nullptr)) {
        ESP_LOGE("MAIN", "Microphone init failed.");
    }

    printf("\nECO Serial Monitor Ready\n");
    printHelp();
    printVoiceHelp();
    if (audioReady) {
        audio.speak(TRACK_SYSTEM_READY);
    }
    printPrompt();

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(serial_task, "serial_task", 6144, NULL, 5, NULL);
    xTaskCreate(microphone_task, "microphone_task", 12288, NULL, 5, NULL);
}

static void sensor_task(void *pvParameters) {
    while (1) {
        sensors.update();
        logSensorAlerts();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void serial_task(void *pvParameters) {
    std::string input;
    input.reserve(128);

    while (1) {
        int ch = fgetc(stdin);
        if (ch == EOF) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            std::string command = normalizeCommand(input.c_str());
            input.clear();

            if (command.empty()) {
                printPrompt();
                continue;
            }

            processCommand(command);
            printPrompt();
            continue;
        }

        if (ch == '\b' || ch == 127) {
            if (!input.empty()) {
                input.pop_back();
            }
            continue;
        }

        if (ch >= 32 && ch <= 126 && input.size() < 127) {
            input.push_back(static_cast<char>(ch));
        }
    }
}

static void processCommand(const std::string& command) {
    if (command == "help") {
        printHelp();
        return;
    }

    if (command == "status") {
        printStatus();
        return;
    }

    if (command == "yes" || command == "yes please") {
        handleConfirmationResponse(true);
        return;
    }

    if (command == "no" || command == "no thanks") {
        handleConfirmationResponse(false);
        return;
    }

    if (handleSensorTestCommand(command)) {
        return;
    }

    if (command == "see mic activity") {
        printMicLogHistory();
        return;
    }

    if (command == "mic log" || command == "mic log on") {
        setLiveMicLog(true);
        return;
    }

    if (command == "mic log off" || command == "stop mic log") {
        setLiveMicLog(false);
        return;
    }

    if (command == "gpio status") {
        appliances.printOutputLevels();
        return;
    }

    if (command == "gpio test") {
        appliances.runOutputDiagnostic();
        return;
    }

    if (command.rfind("pin test ", 0) == 0) {
        const std::string pinText = command.substr(9);
        char *end = nullptr;
        long pin = std::strtol(pinText.c_str(), &end, 10);
        if (end == pinText.c_str() || *end != '\0') {
            printf("Invalid pin number: %s\n", pinText.c_str());
            return;
        }
        appliances.runPinDiagnostic(static_cast<int>(pin));
        return;
    }

    if (tryUnlockWithSecret(command)) {
        return;
    }

    if (command == "led locked" || command == "lock" || command == "lock system") {
        pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };
        if (!appliances.isUnlocked()) {
            printf("System already locked.\n");
        } else {
            appliances.setStatusLED(false);
            audio.speak(TRACK_LOCKED);
            printf("System locked.\n");
        }
        return;
    }

    if (commandRequiresUnlock(command) && !appliances.isUnlocked()) {
        printf("System is locked. Enter the secret code to unlock first.\n");
        audio.speak(TRACK_SYSTEM_LOCKED_MSG);
        return;
    }

    if (command == "light on") {
        handleLightOn();
        return;
    }

    if (command == "light off") {
        handleLightOff();
        return;
    }

    if (command == "fan on") {
        handleFanOn();
        return;
    }

    if (command == "fan off") {
        handleFanOff();
        return;
    }

    if (command == "all off") {
        handleAllOff();
        return;
    }

    printf("Unknown command: %s\n", command.c_str());
    printf("Type 'help' to see available commands.\n");
}

static void handleLightOn() {
    if (appliances.isLightOn()) {
        printf("Light is already ON.\n");
        audio.speak(TRACK_LIGHT_ALREADY_ON);
        return;
    }

    if (!sensors.isMotionDetected()) {
        startConfirmation(PendingActionType::LightOn,
                          "No motion detected. Do you still want to turn on the light? Say yes please or no thanks.",
                          TRACK_NO_MOTION);
        return;
    }

    if (sensors.getLightLevel() > BRIGHTNESS_THRESHOLD) {
        startConfirmation(PendingActionType::LightOn,
                          "It is already bright. Do you still want to switch on the light? Say yes please or no thanks.",
                          TRACK_BRIGHT);
        return;
    }

    performLightOn();
}

static void handleLightOff() {
    pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };
    if (!appliances.isLightOn()) {
        printf("Light is already OFF.\n");
        audio.speak(TRACK_LIGHT_ALREADY_OFF);
        return;
    }

    appliances.setLight(false);
    printf("Light turned OFF.\n");
    audio.speak(TRACK_LIGHT_TURNING_OFF);
}

static void handleFanOn() {
    if (appliances.isFanOn()) {
        printf("Fan is already ON.\n");
        audio.speak(TRACK_FAN_ALREADY_ON);
        return;
    }

    if (!sensors.isMotionDetected()) {
        startConfirmation(PendingActionType::FanOn,
                          "No motion detected. Do you still want to continue with turning on the fan? Say yes please or no thanks.",
                          TRACK_NO_MOTION);
        return;
    }

    if (sensors.getTemperature() < TEMP_LOW_THRESHOLD ||
        sensors.getHumidity() < HUMIDITY_LOW_THRESHOLD) {
        startConfirmation(PendingActionType::FanOn,
                          "Temperature or humidity is below threshold. Do you still want to turn on the fan? Say yes please or no thanks.",
                          TRACK_LOW_TEMP_HUM);
        return;
    }

    performFanOn();
}

static void handleFanOff() {
    pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };
    if (!appliances.isFanOn()) {
        printf("Fan is already OFF.\n");
        audio.speak(TRACK_FAN_ALREADY_OFF);
        return;
    }

    appliances.setFan(false);
    printf("Fan turned OFF.\n");
    audio.speak(TRACK_FAN_TURNING_OFF);
}

static void handleAllOff() {
    pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };
    appliances.turnOffAll();
    printf("All appliances turned OFF.\n");
    audio.speak(TRACK_LIGHT_TURNING_OFF);
    audio.speak(TRACK_FAN_TURNING_OFF);
}

static void performLightOn() {
    pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };
    appliances.setLight(true);
    printf("Light turned ON.\n");
    audio.speak(TRACK_LIGHT_ON);
    reportPowerWarnings();
}

static void performFanOn() {
    pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };
    appliances.setFan(true);
    printf("Fan turned ON.\n");
    audio.speak(TRACK_FAN_ON);
    reportPowerWarnings();
}

static bool commandRequiresUnlock(const std::string& command) {
    return command == "light on" ||
           command == "light off" ||
           command == "fan on" ||
           command == "fan off" ||
           command == "all off";
}

static std::string normalizeCommand(const char *input) {
    std::string command = input;

    command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());
    command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());

    size_t first = command.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return "";
    }

    size_t last = command.find_last_not_of(" \t");
    command = command.substr(first, last - first + 1);

    std::transform(command.begin(), command.end(), command.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return command;
}

static void addMicLogEntry(const MicLogEntry& entry) {
    portENTER_CRITICAL(&micLogMux);
    micLog.push_back(entry);
    const int64_t cutoff = entry.timestampMs - MIC_LOG_RETENTION_MS;
    while (!micLog.empty() && micLog.front().timestampMs < cutoff) {
        micLog.pop_front();
    }
    portEXIT_CRITICAL(&micLogMux);
}

static void printMicLogHistory() {
    int64_t nowMs = esp_timer_get_time() / 1000;
    std::deque<MicLogEntry> snapshot;
    portENTER_CRITICAL(&micLogMux);
    snapshot = micLog;
    portEXIT_CRITICAL(&micLogMux);

    if (snapshot.empty()) {
        printf("\nMic log is empty.\n");
        return;
    }

    printf("\nMic log (last 2 minutes):\n");
    for (const auto &entry : snapshot) {
        int64_t age = nowMs - entry.timestampMs;
        if (age < 0) {
            age = 0;
        }
        printf("  %5lldms ago heard: %s\n", (long long)age, entry.phrase.c_str());
    }
}

static void setLiveMicLog(bool enabled) {
    liveMicLogEnabled = enabled;
    if (enabled) {
        if (!microphone.isReady()) {
            printf("ESP-SR is not ready. Check model flashing and PSRAM configuration.\n");
            liveMicLogEnabled = false;
            return;
        }
        printf("mic log enabled\n");
    } else {
        printf("mic log disabled\n");
    }
}

static bool tryUnlockWithSecret(const std::string& command) {
    static const std::string normalizedSecret = [] {
        std::string code = SECRET_CODE;
        std::transform(code.begin(), code.end(), code.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return code;
    }();

    if (command != normalizedSecret) {
        return false;
    }

    pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };
    if (appliances.isUnlocked()) {
        printf("System already unlocked.\n");
    } else {
        appliances.setStatusLED(true);
        audio.speak(TRACK_UNLOCKED);
        printf("Secret code accepted. System unlocked.\n");
    }
    return true;
}

static void printVoiceHelp() {
    printf("Voice flow:\n");
    printf("  say \"%s\" first, then say one command clearly within %d seconds\n", WAKE_WORD, COMMAND_TIMEOUT_MS / 1000);
    printf("Voice phrases to say exactly:\n");
    printf("  turn on the light             -> Light ON\n");
    printf("  turn off the light            -> Light OFF\n");
    printf("  turn on the air conditioner   -> Fan ON\n");
    printf("  turn off the air conditioner  -> Fan OFF\n");
    printf("  play news channel             -> Show status\n");
    printf("  lock system                   -> Lock system\n");
    printf("  yes please                    -> Confirm a gated action\n");
    printf("  no thanks                     -> Cancel a gated action\n");
    printf("  turn off all the lights       -> All OFF\n");
    printf("  %s -> Unlock system\n\n", SECRET_CODE_PHRASE);
}

static std::string mapVoicePhraseToCommand(const std::string& phrase) {
    if (phrase == SECRET_CODE_PHRASE) {
        return SECRET_CODE;
    }
    if (phrase == "turn on the light") {
        return "light on";
    }
    if (phrase == "turn off the light") {
        return "light off";
    }
    if (phrase == "turn on the air conditioner") {
        return "fan on";
    }
    if (phrase == "turn off the air conditioner") {
        return "fan off";
    }
    if (phrase == "play news channel") {
        return "status";
    }
    if (phrase == "lock system") {
        return "lock system";
    }
    if (phrase == "turn off all the lights") {
        return "all off";
    }
    if (phrase == "yes please") {
        return "yes please";
    }
    if (phrase == "no thanks") {
        return "no thanks";
    }
    return "";
}

static void startConfirmation(PendingActionType action, const char *prompt, AudioTrack advisoryTrack) {
    pendingConfirmation.action = action;
    pendingConfirmation.prompt = prompt;
    pendingConfirmation.advisoryTrack = advisoryTrack;

    printf("%s\n", pendingConfirmation.prompt.c_str());
    if (advisoryTrack != TRACK_NONE) {
        audio.speak(advisoryTrack);
    }
    audio.speak(TRACK_ASK_YES_NO);
}

static void handleConfirmationResponse(bool accepted) {
    if (pendingConfirmation.action == PendingActionType::None) {
        printf("No confirmation is pending.\n");
        return;
    }

    PendingActionType action = pendingConfirmation.action;
    pendingConfirmation = { PendingActionType::None, "", TRACK_NONE };

    if (!accepted) {
        printf("Action cancelled.\n");
        audio.speak(TRACK_ACTION_CANCELLED);
        return;
    }

    printf("Action confirmed.\n");
    audio.speak(TRACK_ACTION_CONFIRMED);

    if (action == PendingActionType::LightOn) {
        performLightOn();
    } else if (action == PendingActionType::FanOn) {
        performFanOn();
    }
}

static bool handleSensorTestCommand(const std::string& command) {
    auto parseFloatValue = [](const std::string& text, float *value) -> bool {
        char *end = nullptr;
        float parsed = std::strtof(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0') {
            return false;
        }
        *value = parsed;
        return true;
    };

    if (command == "test sensors clear") {
        sensors.clearOverrides();
        printf("All sensor test overrides cleared.\n");
        return true;
    }

    if (command == "test motion on") {
        sensors.setMotionOverride(true, true);
        printf("Motion override set to detected.\n");
        return true;
    }

    if (command == "test motion off") {
        sensors.setMotionOverride(true, false);
        printf("Motion override set to not detected.\n");
        return true;
    }

    if (command.rfind("test light ", 0) == 0) {
        float value = 0;
        if (!parseFloatValue(command.substr(11), &value)) {
            printf("Invalid light test value.\n");
            return true;
        }
        sensors.setLightLevelOverride(true, static_cast<int>(value));
        printf("Light level override set to %d.\n", static_cast<int>(value));
        return true;
    }

    if (command.rfind("test temp ", 0) == 0) {
        float value = 0;
        if (!parseFloatValue(command.substr(10), &value)) {
            printf("Invalid temperature test value.\n");
            return true;
        }
        sensors.setTemperatureOverride(true, value);
        printf("Temperature override set to %.1f C.\n", value);
        return true;
    }

    if (command.rfind("test humidity ", 0) == 0) {
        float value = 0;
        if (!parseFloatValue(command.substr(14), &value)) {
            printf("Invalid humidity test value.\n");
            return true;
        }
        sensors.setHumidityOverride(true, value);
        printf("Humidity override set to %.1f %%.\n", value);
        return true;
    }

    if (command.rfind("test voltage ", 0) == 0) {
        float value = 0;
        if (!parseFloatValue(command.substr(13), &value)) {
            printf("Invalid voltage test value.\n");
            return true;
        }
        sensors.setVoltageOverride(true, value);
        printf("Voltage override set to %.2f V.\n", value);
        return true;
    }

    if (command.rfind("test current ", 0) == 0) {
        float value = 0;
        if (!parseFloatValue(command.substr(13), &value)) {
            printf("Invalid current test value.\n");
            return true;
        }
        sensors.setCurrentOverride(true, value);
        printf("Current override set to %.2f A.\n", value);
        return true;
    }

    if (command.rfind("test power ", 0) == 0) {
        float value = 0;
        if (!parseFloatValue(command.substr(11), &value)) {
            printf("Invalid power test value.\n");
            return true;
        }
        sensors.setPowerOverride(true, value);
        printf("Power override set to %.2f W.\n", value);
        return true;
    }

    return false;
}

static void logSensorAlerts() {
    static bool lastVoltageLow = false;
    static bool lastVoltageFluctuating = false;
    static bool lastCurrentFluctuating = false;
    static bool lastOvercurrent = false;

    const bool voltageLow = sensors.isVoltageLow();
    const bool voltageFluctuating = sensors.isVoltageFluctuating();
    const bool currentFluctuating = sensors.isCurrentFluctuating();
    const bool overcurrent = sensors.isOvercurrent();

    if (voltageLow && !lastVoltageLow) {
        printf("[sensor] Low voltage detected: %.2f V\n", sensors.getVoltage());
        audio.speak(TRACK_LOW_VOLTAGE);
    }
    if (voltageFluctuating && !lastVoltageFluctuating) {
        printf("[sensor] Voltage fluctuation detected: %.2f V\n", sensors.getVoltage());
        audio.speak(TRACK_VOLT_FLUCTUATION);
    }
    if (currentFluctuating && !lastCurrentFluctuating) {
        printf("[sensor] Load current fluctuation detected: %.2f A\n", sensors.getCurrent());
        audio.speak(TRACK_OVERCURRENT);
    }
    if (overcurrent && !lastOvercurrent) {
        printf("[sensor] Overcurrent detected: %.2f A\n", sensors.getCurrent());
        audio.speak(TRACK_OVERCURRENT);
    }

    lastVoltageLow = voltageLow;
    lastVoltageFluctuating = voltageFluctuating;
    lastCurrentFluctuating = currentFluctuating;
    lastOvercurrent = overcurrent;
}

static void reportPowerWarnings() {
    if (sensors.isVoltageLow()) {
        printf("Warning: low voltage detected (%.2f V).\n", sensors.getVoltage());
    }
    if (sensors.isVoltageFluctuating()) {
        printf("Warning: voltage fluctuation detected.\n");
    }
    if (sensors.isCurrentFluctuating()) {
        printf("Warning: load current is fluctuating.\n");
    }
    if (sensors.isOvercurrent()) {
        printf("Warning: load current is above threshold.\n");
    }
}

static void printHelp() {
    printf("\nAvailable commands:\n");
    printf("  help               - Show this help menu\n");
    printf("  status             - Print current sensor and relay status\n");
    printf("  light on/off       - Control light relay when unlocked\n");
    printf("  fan on/off         - Control fan relay when unlocked\n");
    printf("  all off            - Turn off light and fan when unlocked\n");
    printf("  yes please / no thanks - Confirm or cancel a pending gated action\n");
    printf("  lock               - Force system lock\n");
    printf("  <secret code>      - Unlock system\n");
    printf("  see mic activity   - Print recognized speech history\n");
    printf("  mic log            - Start live recognition diagnostics\n");
    printf("  mic log off        - Stop live recognition diagnostics\n");
    printf("  gpio status        - Print output GPIO levels\n");
    printf("  gpio test          - Toggle each output pin directly\n");
    printf("  pin test N         - Toggle a specific GPIO directly\n");
    printf("  test motion on/off - Override PIR state\n");
    printf("  test light N       - Override LDR value\n");
    printf("  test temp N        - Override temperature in C\n");
    printf("  test humidity N    - Override humidity in %%\n");
    printf("  test voltage N     - Override voltage in V\n");
    printf("  test current N     - Override current in A\n");
    printf("  test power N       - Override power in W\n");
    printf("  test sensors clear - Clear all sensor overrides\n\n");
}

static void printPrompt() {
    printf("> ");
    fflush(stdout);
}

static void printStatus() {
    printf("\nSystem status:\n");
    printf("  Motion             : %s\n", sensors.isMotionDetected() ? "detected" : "not detected");
    printf("  Light level        : %d\n", sensors.getLightLevel());
    printf("  Temperature        : %.1f C (%s)\n", sensors.getTemperature(),
           sensors.isDht11Available() ? "DHT11 OK" : "DHT11 unavailable");
    printf("  Humidity           : %.1f %%\n", sensors.getHumidity());
    printf("  Current            : %.2f A (%s)\n", sensors.getCurrent(),
           sensors.isIna219Available() ? "INA219 OK" : "INA219 unavailable");
    printf("  Voltage            : %.2f V\n", sensors.getVoltage());
    printf("  Power              : %.2f W\n", sensors.getPower());
    printf("  Light relay        : %s\n", appliances.isLightOn() ? "ON" : "OFF");
    printf("  Fan relay          : %s\n", appliances.isFanOn() ? "ON" : "OFF");
    printf("  Voltage low        : %s\n", sensors.isVoltageLow() ? "YES" : "NO");
    printf("  Voltage fluctuation: %s\n", sensors.isVoltageFluctuating() ? "YES" : "NO");
    printf("  Current fluctuation: %s\n", sensors.isCurrentFluctuating() ? "YES" : "NO");
    printf("  Overcurrent        : %s\n", sensors.isOvercurrent() ? "YES" : "NO");
    printf("  Pending confirm    : %s\n",
           pendingConfirmation.action == PendingActionType::None ? "NONE" : pendingConfirmation.prompt.c_str());
    printf("  System lock        : %s\n\n", appliances.isUnlocked() ? "UNLOCKED" : "LOCKED");
}

static void microphone_task(void *pvParameters) {
    bool previousSpeechActive = false;
    int64_t lastDiagnosticLogMs = 0;
    int64_t commandWindowDeadlineMs = 0;

    while (1) {
        if (!microphone.isReady()) {
            appliances.setActivityLED(false);
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        std::string phrase = microphone.pollRecognizedPhrase();
        const bool speechActive = microphone.detectSound();

        appliances.setActivityLED(speechActive);

        if (speechActive != previousSpeechActive) {
            printf("[voice] %s (slot=%d level=%d threshold=%d peak=%d rawL=%d rawR=%d)\n",
                   speechActive ? "sound detected" : "sound ended",
                   microphone.getActiveSlot(),
                   microphone.getLastLevel(),
                   microphone.getDynamicThreshold(),
                   microphone.getPeakToPeak(),
                   microphone.getLastSlot0Level(),
                   microphone.getLastSlot1Level());
            previousSpeechActive = speechActive;
        }

        if (liveMicLogEnabled) {
            const int64_t nowMs = esp_timer_get_time() / 1000;
            if ((nowMs - lastDiagnosticLogMs) >= SOUND_LOG_INTERVAL_MS) {
                printf("[voice] monitor slot=%d level=%d threshold=%d peak=%d rawL=%d rawR=%d%s\n",
                       microphone.getActiveSlot(),
                       microphone.getLastLevel(),
                       microphone.getDynamicThreshold(),
                       microphone.getPeakToPeak(),
                       microphone.getLastSlot0Level(),
                       microphone.getLastSlot1Level(),
                       microphone.isCalibrating() ? " calibrating" : "");
                lastDiagnosticLogMs = nowMs;
            }
        }

        if (!phrase.empty()) {
            MicLogEntry entry;
            entry.timestampMs = esp_timer_get_time() / 1000;
            entry.phrase = phrase;
            addMicLogEntry(entry);

            printf("[voice] recognized: %s\n", phrase.c_str());

            if (phrase == WAKE_WORD) {
                commandWindowDeadlineMs = entry.timestampMs + COMMAND_TIMEOUT_MS;
                printf("[voice] wake word accepted, listening for command\n");
                audio.speak(TRACK_MIC_ACTIVATED);
                audio.speak(TRACK_LISTENING_CMD);
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            const bool windowOpen = entry.timestampMs <= commandWindowDeadlineMs;
            const bool bypassWakeWindow =
                (phrase == SECRET_CODE_PHRASE) ||
                (phrase == "yes please") ||
                (phrase == "no thanks");

            if (!windowOpen && !bypassWakeWindow) {
                if (liveMicLogEnabled) {
                    printf("[voice] ignored outside wake window: %s\n", phrase.c_str());
                }
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            const std::string command = mapVoicePhraseToCommand(phrase);
            if (!command.empty()) {
                printf("[voice] executing: %s\n", command.c_str());
                processCommand(normalizeCommand(command.c_str()));
                commandWindowDeadlineMs = 0;
                printPrompt();
            } else if (liveMicLogEnabled) {
                printf("[voice] No action mapped for: %s\n", phrase.c_str());
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
