/*
 * Appliance Control Implementation
 */

#include "appliance_control.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
    gpio_set_level((gpio_num_t)RELAY_LIGHT_PIN, 0);
    gpio_set_level((gpio_num_t)RELAY_FAN_PIN, 0);

    // Both LEDs off at startup — system is sleeping, waiting for wake word
    gpio_set_level((gpio_num_t)LED_GREEN_PIN, 0);
    gpio_set_level((gpio_num_t)LED_RED_PIN, 0);
    ESP_LOGI("APPLIANCE", "Status: IDLE (LEDs OFF — waiting for wake word)");

    ESP_LOGI("APPLIANCE", "Appliance Control Ready");
}

void ApplianceControl::setLight(bool state) {
    lightState = state;
    gpio_set_level((gpio_num_t)RELAY_LIGHT_PIN, state ? 1 : 0);
    ESP_LOGI("APPLIANCE", "Light turned %s", state ? "ON" : "OFF");
}

void ApplianceControl::setFan(bool state) {
    fanState = state;
    gpio_set_level((gpio_num_t)RELAY_FAN_PIN, state ? 1 : 0);
    ESP_LOGI("APPLIANCE", "Fan turned %s", state ? "ON" : "OFF");
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
    // Relay is active HIGH per spec (High=ON, Low=OFF)
    gpio_set_level((gpio_num_t)pin, state ? 1 : 0);
}

void ApplianceControl::setStatusLED(bool unlocked) {
    unlockedState = unlocked;
    if (unlocked) {
        // System unlocked - Green LED ON, Red LED OFF
        gpio_set_level((gpio_num_t)LED_GREEN_PIN, 1);
        gpio_set_level((gpio_num_t)LED_RED_PIN, 0);
        ESP_LOGI("APPLIANCE", "Status: UNLOCKED (Green LED)");
    } else {
        // System locked/idle — both LEDs OFF (sleeping, waiting for wake word)
        gpio_set_level((gpio_num_t)LED_GREEN_PIN, 0);
        gpio_set_level((gpio_num_t)LED_RED_PIN, 0);
        ESP_LOGI("APPLIANCE", "Status: LOCKED (LEDs OFF)");
    }
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
