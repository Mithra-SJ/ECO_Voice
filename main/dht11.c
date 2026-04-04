#include "dht11.h"

#include <stdint.h>
#include "esp_rom_sys.h"
#include "esp_timer.h"

static gpio_num_t s_dht11_pin = GPIO_NUM_NC;

static void dht11_set_output(int level) {
    gpio_set_direction(s_dht11_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(s_dht11_pin, level);
}

static void dht11_set_input(void) {
    gpio_set_direction(s_dht11_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(s_dht11_pin, GPIO_PULLUP_ONLY);
}

static int dht11_wait_for_level(int expected_level, int timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(s_dht11_pin) != expected_level) {
        if ((esp_timer_get_time() - start) >= timeout_us) {
            return 0;
        }
    }
    return 1;
}

static int dht11_measure_high_pulse_us(int timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(s_dht11_pin) == 1) {
        if ((esp_timer_get_time() - start) >= timeout_us) {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start);
}

void DHT11_init(gpio_num_t pin) {
    s_dht11_pin = pin;
    gpio_reset_pin(s_dht11_pin);
    dht11_set_output(1);
}

struct dht11_reading DHT11_read(void) {
    struct dht11_reading reading = {
        .status = DHT11_TIMEOUT,
        .temperature = 0,
        .humidity = 0,
    };
    uint8_t data[5] = {0};

    if (s_dht11_pin == GPIO_NUM_NC) {
        return reading;
    }

    dht11_set_output(0);
    esp_rom_delay_us(20000);
    dht11_set_output(1);
    esp_rom_delay_us(40);
    dht11_set_input();

    if (!dht11_wait_for_level(0, 100)) {
        return reading;
    }
    if (!dht11_wait_for_level(1, 100)) {
        return reading;
    }
    if (!dht11_wait_for_level(0, 100)) {
        return reading;
    }

    for (int i = 0; i < 40; ++i) {
        if (!dht11_wait_for_level(1, 70)) {
            return reading;
        }

        int high_time = dht11_measure_high_pulse_us(120);
        if (high_time < 0) {
            return reading;
        }

        data[i / 8] <<= 1;
        if (high_time > 40) {
            data[i / 8] |= 1;
        }
    }

    dht11_set_output(1);

    if (((uint8_t)(data[0] + data[1] + data[2] + data[3])) != data[4]) {
        reading.status = DHT11_CHECKSUM_ERROR;
        return reading;
    }

    reading.status = DHT11_OK;
    reading.humidity = data[0];
    reading.temperature = data[2];
    return reading;
}
