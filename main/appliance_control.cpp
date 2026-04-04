/*
 * Appliance Control Implementation
 */

#include "appliance_control.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if RELAY_ACTIVE_LOW
static constexpr int RELAY_ON_LEVEL = 0;
static constexpr int RELAY_OFF_LEVEL = 1;
#else
static constexpr int RELAY_ON_LEVEL = 1;
static constexpr int RELAY_OFF_LEVEL = 0;
#endif

ApplianceControl::ApplianceControl() :
    lightState(false),
    fanState(false),
    unlockedState(false) {
}

void ApplianceControl::init() {
    ESP_LOGI("APPLIANCE", "Initializing Appliance Control...");

    // Configure relay pins as outputs
    gpio_set_direction((gpio_num_t)RELAY_LIGHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)RELAY_FAN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)LED_GREEN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)LED_RED_PIN, GPIO_MODE_OUTPUT);

    // Initialize relays off
    gpio_set_level((gpio_num_t)RELAY_LIGHT_PIN, RELAY_OFF_LEVEL);
    gpio_set_level((gpio_num_t)RELAY_FAN_PIN, RELAY_OFF_LEVEL);

    // Set initial LED state
    setStatusLED(false); // locked = red LED ON

    ESP_LOGI("APPLIANCE", "Appliance Control Ready");
}

void ApplianceControl::setLight(bool state) {
    lightState = state;
    gpio_set_level((gpio_num_t)RELAY_LIGHT_PIN, state ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
    ESP_LOGI("APPLIANCE", "Light turned %s (GPIO%d=%d)",
             state ? "ON" : "OFF", RELAY_LIGHT_PIN, gpio_get_level((gpio_num_t)RELAY_LIGHT_PIN));
}

void ApplianceControl::setFan(bool state) {
    fanState = state;
    gpio_set_level((gpio_num_t)RELAY_FAN_PIN, state ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
    ESP_LOGI("APPLIANCE", "Fan turned %s (GPIO%d=%d)",
             state ? "ON" : "OFF", RELAY_FAN_PIN, gpio_get_level((gpio_num_t)RELAY_FAN_PIN));
}

void ApplianceControl::turnOffAll() {
    setLight(false);
    setFan(false);
    ESP_LOGI("APPLIANCE", "All appliances turned OFF");
}

bool ApplianceControl::isLightOn() {
    return lightState;
}

bool ApplianceControl::isFanOn() {
    return fanState;
}

void ApplianceControl::updateRelay(int pin, bool state) {
    gpio_set_level((gpio_num_t)pin, state ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

void ApplianceControl::setStatusLED(bool unlocked) {
    unlockedState = unlocked;
    gpio_set_level((gpio_num_t)LED_RED_PIN, unlocked ? 1 : 0);
    ESP_LOGI("APPLIANCE", "Status: %s (GPIO%d=%d)",
             unlocked ? "UNLOCKED" : "LOCKED",
             LED_RED_PIN, gpio_get_level((gpio_num_t)LED_RED_PIN));
}

void ApplianceControl::blinkStatusLED(bool green, int times) {
    int ledPin = green ? LED_GREEN_PIN : LED_RED_PIN;

    for (int i = 0; i < times; i++) {
        gpio_set_level((gpio_num_t)ledPin, 1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        gpio_set_level((gpio_num_t)ledPin, 0);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    // Restore LED to the correct lock/unlock state
    setStatusLED(unlockedState);
}

void ApplianceControl::setActivityLED(bool on) {
    gpio_set_level((gpio_num_t)LED_GREEN_PIN, on ? 1 : 0);
    if (!on) {
        gpio_set_level((gpio_num_t)LED_RED_PIN, unlockedState ? 1 : 0);
    }
}

void ApplianceControl::printOutputLevels() {
    printf("GPIO levels:\n");
    printf("  Light relay pin GPIO%d = %d\n", RELAY_LIGHT_PIN, gpio_get_level((gpio_num_t)RELAY_LIGHT_PIN));
    printf("  Fan relay pin   GPIO%d = %d\n", RELAY_FAN_PIN, gpio_get_level((gpio_num_t)RELAY_FAN_PIN));
    printf("  Green LED pin   GPIO%d = %d\n", LED_GREEN_PIN, gpio_get_level((gpio_num_t)LED_GREEN_PIN));
    printf("  Red LED pin     GPIO%d = %d\n", LED_RED_PIN, gpio_get_level((gpio_num_t)LED_RED_PIN));
}

void ApplianceControl::runOutputDiagnostic() {
    struct OutputStep {
        int pin;
        const char *name;
    };

    const OutputStep outputs[] = {
        { LED_GREEN_PIN, "Green LED" },
        { LED_RED_PIN, "Red LED" },
        { RELAY_LIGHT_PIN, "Light relay" },
        { RELAY_FAN_PIN, "Fan relay" },
    };

    printf("Starting output diagnostic...\n");
    for (const auto &output : outputs) {
        printf("Testing %s on GPIO%d\n", output.name, output.pin);
        gpio_set_level((gpio_num_t)output.pin, 1);
        printf("  level after HIGH = %d\n", gpio_get_level((gpio_num_t)output.pin));
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level((gpio_num_t)output.pin, 0);
        printf("  level after LOW  = %d\n", gpio_get_level((gpio_num_t)output.pin));
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    lightState = false;
    fanState = false;
    setStatusLED(unlockedState);
    printf("Output diagnostic complete.\n");
}

void ApplianceControl::runPinDiagnostic(int pin) {
    if (pin < 0 || pin > 48) {
        printf("Invalid GPIO%d. Enter a GPIO between 0 and 48.\n", pin);
        return;
    }

    gpio_num_t gpioPin = (gpio_num_t)pin;
    esp_err_t resetErr = gpio_reset_pin(gpioPin);
    if (resetErr != ESP_OK) {
        printf("Failed to reset GPIO%d: %s\n", pin, esp_err_to_name(resetErr));
        return;
    }

    esp_err_t dirErr = gpio_set_direction(gpioPin, GPIO_MODE_OUTPUT);
    if (dirErr != ESP_OK) {
        printf("Failed to set GPIO%d as output: %s\n", pin, esp_err_to_name(dirErr));
        return;
    }

    printf("Testing GPIO%d as direct output...\n", pin);

    gpio_set_level(gpioPin, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    printf("  level after HIGH = %d\n", gpio_get_level(gpioPin));

    gpio_set_level(gpioPin, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    printf("  level after LOW  = %d\n", gpio_get_level(gpioPin));

    printf("GPIO%d test complete.\n", pin);
}

bool ApplianceControl::isUnlocked() const {
    return unlockedState;
}
