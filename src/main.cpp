#include <stdio.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"
#include "sensor_handler.h"
#include "appliance_control.h"
#include "audio_handler.h"

static SensorHandler sensors;
static ApplianceControl appliances;
static AudioHandler audio;

static void sensor_task(void *pvParameters);
static void serial_task(void *pvParameters);
static void printHelp();
static void printPrompt();
static void printStatus();
static void processCommand(const std::string& command);
static std::string normalizeCommand(const char *input);
static void handleLightOn();
static void handleLightOff();
static void handleFanOn();
static void handleFanOff();
static void handleAllOff();

extern "C" void app_main(void) {
    ESP_LOGI("MAIN", "=== ECO Serial Monitor Starting ===");

    if (!sensors.init()) {
        ESP_LOGE("MAIN", "Sensor init failed. Check INA219 wiring.");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    appliances.init();
    if (!audio.init()) {
        ESP_LOGW("MAIN", "DFPlayer init failed. Audio prompts are disabled.");
    }

    printf("\nECO Serial Monitor Ready\n");
    audio.speak(TRACK_SYSTEM_READY);
    printHelp();
    printPrompt();

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(serial_task, "serial_task", 6144, NULL, 5, NULL);
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

    if (command == "led locked") {
        appliances.setStatusLED(false);
        printf("Status LED set to LOCKED.\n");
        return;
    }

    if (command == "led unlocked") {
        appliances.setStatusLED(true);
        printf("Status LED set to UNLOCKED.\n");
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
    printf("  led locked   - Show locked state on status LEDs\n");
    printf("  led unlocked - Show unlocked state on status LEDs\n\n");
    printf("  gpio status  - Print output GPIO levels\n");
    printf("  gpio test    - Toggle each output pin directly\n\n");
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
    printf("  Voltage var : %s\n\n", sensors.isVoltageFluctuating() ? "YES" : "NO");
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
