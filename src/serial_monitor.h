#ifndef SERIAL_MONITOR_H
#define SERIAL_MONITOR_H

#include <stdio.h>
#include "esp_log.h"

#define SERIAL_STEP(tag, fmt, ...) \
    do { \
        ESP_LOGI(tag, fmt, ##__VA_ARGS__); \
        printf("[SERIAL][%s] " fmt "\n", tag, ##__VA_ARGS__); \
        fflush(stdout); \
    } while (0)

#define SERIAL_WARN(tag, fmt, ...) \
    do { \
        ESP_LOGW(tag, fmt, ##__VA_ARGS__); \
        printf("[SERIAL][%s][WARN] " fmt "\n", tag, ##__VA_ARGS__); \
        fflush(stdout); \
    } while (0)

#define SERIAL_ERROR(tag, fmt, ...) \
    do { \
        ESP_LOGE(tag, fmt, ##__VA_ARGS__); \
        printf("[SERIAL][%s][ERROR] " fmt "\n", tag, ##__VA_ARGS__); \
        fflush(stdout); \
    } while (0)

#endif
