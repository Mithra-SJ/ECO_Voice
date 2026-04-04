#ifndef DHT11_H
#define DHT11_H

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DHT11_OK = 0,
    DHT11_TIMEOUT = -1,
    DHT11_CHECKSUM_ERROR = -2
} dht11_status_t;

struct dht11_reading {
    int status;
    int temperature;
    int humidity;
};

void DHT11_init(gpio_num_t pin);
struct dht11_reading DHT11_read(void);

#ifdef __cplusplus
}
#endif

#endif
