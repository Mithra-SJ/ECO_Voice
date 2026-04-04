#include <stdio.h>
#include <string>
#include <deque>
#include <cstdint>
#include <algorithm>
#include <cctype>
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

static std::deque<MicLogEntry> micLog;
static portMUX_TYPE micLogMux = portMUX_INITIALIZER_UNLOCKED;
static constexpr int64_t MIC_LOG_RETENTION_MS = 2 * 60 * 1000;
static volatile bool liveMicLogEnabled = false;

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
static std::string normalizeCommand(const char *input);
static void addMicLogEntry(const MicLogEntry& entry);
static void printMicLogHistory();
static void setLiveMicLog(bool enabled);
static bool tryUnlockWithSecret(const std::string& command);

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
    if (audioReady) {
        audio.speak(TRACK_SYSTEM_READY);
    }
    printPrompt();

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(serial_task, "serial_task", 6144, NULL, 5, NULL);
    xTaskCreate(microphone_task, "microphone_task", 4096, NULL, 5, NULL);
}

static void sensor_task(void *pvParameters) {
    while (1) {
        sensors.update();
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

        if (ch >= 32 && ch <= 126) {
            if (input.size() < 127) {
                input.push_back(static_cast<char>(ch));
            }
            continue;
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

    if (command == "led locked" || command == "lock" || command == "lock system") {
        if (!appliances.isUnlocked()) {
            printf("System already locked. Red LED is off.\n");
        } else {
            appliances.setStatusLED(false);
            audio.speak(TRACK_LOCKED);
            printf("System locked. Red LED is off.\n");
        }
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

    printf("Unknown command: %s\n", command.c_str());
    printf("Type 'help' to see available commands.\n");
}

static void handleLightOn() {
    if (!sensors.isMotionDetected()) {
        printf("Warning: no motion detected.\n");
    }

    if (sensors.getLightLevel() > BRIGHTNESS_THRESHOLD) {
        printf("Warning: ambient light is already above threshold.\n");
    }

    if (sensors.isVoltageLow()) {
        printf("Warning: low voltage detected.\n");
    }

    if (sensors.isVoltageFluctuating()) {
        printf("Warning: voltage fluctuation detected.\n");
    }

    appliances.setLight(true);
    printf("Light turned ON.\n");
    audio.speak(TRACK_LIGHT_ON);
}

static void handleLightOff() {
    appliances.setLight(false);
    printf("Light turned OFF.\n");
    audio.speak(TRACK_LIGHT_TURNING_OFF);
}

static void handleFanOn() {
    if (!sensors.isMotionDetected()) {
        printf("Warning: no motion detected.\n");
    }

    if (sensors.getTemperature() < TEMP_LOW_THRESHOLD ||
        sensors.getHumidity() < HUMIDITY_LOW_THRESHOLD) {
        printf("Warning: temperature or humidity is below the fan recommendation threshold.\n");
    }

    if (sensors.isVoltageLow()) {
        printf("Warning: low voltage detected.\n");
    }

    if (sensors.isVoltageFluctuating()) {
        printf("Warning: voltage fluctuation detected.\n");
    }

    appliances.setFan(true);
    printf("Fan turned ON.\n");
    audio.speak(TRACK_FAN_ON);
}

static void handleFanOff() {
    appliances.setFan(false);
    printf("Fan turned OFF.\n");
    audio.speak(TRACK_FAN_TURNING_OFF);
}

static void handleAllOff() {
    appliances.turnOffAll();
    printf("All appliances turned OFF.\n");
    audio.speak(TRACK_LIGHT_TURNING_OFF);
    audio.speak(TRACK_FAN_TURNING_OFF);
}

static void printHelp() {
    printf("\nAvailable commands:\n");
    printf("  help         - Show this help menu\n");
    printf("  status       - Print current sensor and relay status\n");
    printf("  light on     - Turn the light relay on\n");
    printf("  light off    - Turn the light relay off\n");
    printf("  fan on       - Turn the fan relay on\n");
    printf("  fan off      - Turn the fan relay off\n");
    printf("  all off      - Turn off light and fan\n");
    printf("  lock (or led locked) - Force system lock (red LED off)\n");
    printf("  <secret code>        - Enter your SECRET_CODE to unlock (red LED on)\n");
    printf("  see mic activity - Print the last two minutes of recognized speech history\n");
    printf("  mic log          - Start live predefined-word recognition in the serial monitor\n");
    printf("  mic log off      - Stop live word recognition\n\n");
    printf("  gpio status  - Print output GPIO levels\n");
    printf("  gpio test    - Toggle each output pin directly\n");
    printf("  pin test N   - Toggle a specific GPIO directly\n\n");
}

static void printPrompt() {
    printf("> ");
    fflush(stdout);
}

static void printStatus() {
    printf("\nSystem status:\n");
    printf("  Motion      : %s\n", sensors.isMotionDetected() ? "detected" : "not detected");
    printf("  Light level : %d\n", sensors.getLightLevel());
    printf("  Temperature : %.1f C\n", sensors.getTemperature());
    printf("  Humidity    : %.1f %%\n", sensors.getHumidity());
    printf("  Current     : %.2f A\n", sensors.getCurrent());
    printf("  Voltage     : %.2f V\n", sensors.getVoltage());
    printf("  Power       : %.2f W\n", sensors.getPower());
    printf("  Light relay : %s\n", appliances.isLightOn() ? "ON" : "OFF");
    printf("  Fan relay   : %s\n", appliances.isFanOn() ? "ON" : "OFF");
    printf("  Voltage low : %s\n", sensors.isVoltageLow() ? "YES" : "NO");
    printf("  Voltage var : %s\n", sensors.isVoltageFluctuating() ? "YES" : "NO");
    printf("  System lock : %s\n\n", appliances.isUnlocked() ? "UNLOCKED" : "LOCKED");
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
        if (age < 0) age = 0;
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
        printf("Live mic log enabled. Speak one of the predefined words or phrases.\n");
        printf("Type 'mic log off' to stop live recognition.\n");
    } else {
        printf("Live mic log disabled.\n");
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

    if (appliances.isUnlocked()) {
        printf("System already unlocked.\n");
    } else {
        appliances.setStatusLED(true);
        audio.speak(TRACK_UNLOCKED);
        printf("Secret code accepted. System unlocked (red LED on).\n");
    }
    return true;
}

static void microphone_task(void *pvParameters) {
    bool speechWasActive = false;
    int64_t lastSpeechReportMs = 0;

    while (1) {
        if (!liveMicLogEnabled) {
            appliances.setActivityLED(false);
            speechWasActive = false;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        std::string phrase = microphone.pollRecognizedPhrase();
        const bool speechActive = microphone.detectSound();
        const int64_t nowMs = esp_timer_get_time() / 1000;

        appliances.setActivityLED(speechActive);

        if (!phrase.empty()) {
            printf("Heard: %s\n", phrase.c_str());
            speechWasActive = true;
            lastSpeechReportMs = nowMs;

            MicLogEntry entry;
            entry.timestampMs = esp_timer_get_time() / 1000;
            entry.phrase = phrase;
            addMicLogEntry(entry);
        } else if (speechActive && (!speechWasActive || (nowMs - lastSpeechReportMs) >= 1000)) {
            printf("Speech detected. level=%d p2p=%d\n",
                   microphone.getLastLevel(), microphone.getPeakToPeak());
            speechWasActive = true;
            lastSpeechReportMs = nowMs;
        } else if (!speechActive) {
            speechWasActive = false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
