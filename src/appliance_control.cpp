/*
 * Appliance Control Implementation
 */

#include "appliance_control.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "serial_monitor.h"

ApplianceControl::ApplianceControl() :
    lightState(false),
    fanState(false),
    unlockedState(false) {
}

void ApplianceControl::init() {
    SERIAL_STEP("APPLIANCE", "Initializing appliance control");

    gpio_set_direction((gpio_num_t)RELAY_LIGHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)RELAY_FAN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)LED_GREEN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)LED_RED_PIN, GPIO_MODE_OUTPUT);

    gpio_set_level((gpio_num_t)RELAY_LIGHT_PIN, 0);
    gpio_set_level((gpio_num_t)RELAY_FAN_PIN, 0);
    gpio_set_level((gpio_num_t)LED_GREEN_PIN, 0);
    gpio_set_level((gpio_num_t)LED_RED_PIN, 0);

    SERIAL_STEP("APPLIANCE", "Status: IDLE (LEDs OFF, waiting for wake word)");
    SERIAL_STEP("APPLIANCE", "Appliance control ready");
}

void ApplianceControl::setLight(bool state) {
    lightState = state;
    gpio_set_level((gpio_num_t)RELAY_LIGHT_PIN, state ? 1 : 0);
    SERIAL_STEP("APPLIANCE", "Light relay on GPIO %d turned %s", RELAY_LIGHT_PIN, state ? "ON" : "OFF");
}

void ApplianceControl::setFan(bool state) {
    fanState = state;
    gpio_set_level((gpio_num_t)RELAY_FAN_PIN, state ? 1 : 0);
    SERIAL_STEP("APPLIANCE", "Fan relay on GPIO %d turned %s", RELAY_FAN_PIN, state ? "ON" : "OFF");
}

void ApplianceControl::turnOffAll() {
    setLight(false);
    setFan(false);
    SERIAL_STEP("APPLIANCE", "All appliances turned OFF");
}

bool ApplianceControl::isLightOn() {
    return lightState;
}

bool ApplianceControl::isFanOn() {
    return fanState;
}

void ApplianceControl::updateRelay(int pin, bool state) {
    gpio_set_level((gpio_num_t)pin, state ? 1 : 0);
}

void ApplianceControl::setStatusLED(bool unlocked) {
    unlockedState = unlocked;
    if (unlocked) {
        gpio_set_level((gpio_num_t)LED_GREEN_PIN, 1);
        gpio_set_level((gpio_num_t)LED_RED_PIN, 0);
        SERIAL_STEP("APPLIANCE", "Status: UNLOCKED (Green LED ON)");
    } else {
        gpio_set_level((gpio_num_t)LED_GREEN_PIN, 0);
        gpio_set_level((gpio_num_t)LED_RED_PIN, 0);
        SERIAL_STEP("APPLIANCE", "Status: LOCKED (LEDs OFF)");
    }
}

void ApplianceControl::blinkStatusLED(bool green, int times) {
    int ledPin = green ? LED_GREEN_PIN : LED_RED_PIN;
    SERIAL_STEP("APPLIANCE", "Blinking %s LED %d time(s)", green ? "GREEN" : "RED", times);

    for (int i = 0; i < times; i++) {
        gpio_set_level((gpio_num_t)ledPin, 1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        gpio_set_level((gpio_num_t)ledPin, 0);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    setStatusLED(unlockedState);
}
