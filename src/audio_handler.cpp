/*
 * Audio Handler Implementation — DFPlayer Mini (ESP-IDF, direct UART protocol)
 *
 * Drives DFPlayer Mini over UART1 without any Arduino library.
 * Pins: DFPLAYER_TX_PIN (ESP32 TX → DFPlayer RX)
 *       DFPLAYER_RX_PIN (ESP32 RX ← DFPlayer TX)
 *       DFPLAYER_BUSY_PIN (DFPlayer BUSY, LOW=playing, HIGH=idle)
 *
 * Supported DFPlayer commands used here:
 *   0x03 — Play track N (root folder)
 *   0x06 — Set volume (0–30)
 *   0x0C — Reset
 *   0x09 — Specify storage source (TF card = 0x01)
 */

#include "audio_handler.h"
#include "config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "AUDIO";
#define DFPLAYER_UART_NUM  UART_NUM_1

// ---------------------------------------------------------------------------
// Private: build and send one 10-byte DFPlayer frame over UART1
// ---------------------------------------------------------------------------
void AudioHandler::sendFrame(uint8_t cmd, uint8_t param1, uint8_t param2) {
    // Checksum covers bytes [1..6]: 0xFF 0x06 CMD 0x00 P1 P2
    uint16_t sum = 0xFF + 0x06 + cmd + 0x00 + param1 + param2;
    uint16_t checksum = (uint16_t)(~sum + 1);  // 2's complement

    uint8_t frame[10] = {
        0x7E,                        // start byte
        0xFF,                        // version
        0x06,                        // data length
        cmd,                         // command
        0x00,                        // no feedback
        param1,
        param2,
        (uint8_t)(checksum >> 8),    // checksum high byte
        (uint8_t)(checksum & 0xFF),  // checksum low byte
        0xEF                         // end byte
    };
    uart_write_bytes(DFPLAYER_UART_NUM, (const char *)frame, sizeof(frame));
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
AudioHandler::AudioHandler() : initialized(false) {}

void AudioHandler::logResponse(const char* message) {
    if (!message || !message[0]) return;
    ESP_LOGI("REPLY", "%s", message);
    printf("[REPLY] %s\n", message);
}

// ---------------------------------------------------------------------------
// init — install UART1, reset DFPlayer, set volume
// ---------------------------------------------------------------------------
bool AudioHandler::init() {
#if !ENABLE_DFPLAYER
    ESP_LOGI(TAG, "DFPlayer disabled in config. Using Serial Monitor replies only.");
    initialized = false;
    return true;
#else
    ESP_LOGI(TAG, "Initializing DFPlayer Mini on UART1 (TX=%d, RX=%d)...",
             DFPLAYER_TX_PIN, DFPLAYER_RX_PIN);

    uart_config_t uart_cfg = {
        .baud_rate  = 9600,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };

    esp_err_t err;
    err = uart_driver_install(DFPLAYER_UART_NUM, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART install failed: %s", esp_err_to_name(err));
        return false;
    }
    err = uart_param_config(DFPLAYER_UART_NUM, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART config failed: %s", esp_err_to_name(err));
        return false;
    }
    err = uart_set_pin(DFPLAYER_UART_NUM,
                       DFPLAYER_TX_PIN, DFPLAYER_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        return false;
    }

    // BUSY pin: LOW = DFPlayer is playing, HIGH = idle
    gpio_set_direction((gpio_num_t)DFPLAYER_BUSY_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)DFPLAYER_BUSY_PIN, GPIO_PULLUP_ONLY);

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // DFPlayer power-on delay

    sendFrame(0x0C, 0x00, 0x00);            // Reset DFPlayer
    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Wait for reset to complete

    sendFrame(0x09, 0x00, 0x01);            // CMD 0x09: select TF card as source (0x01)
    vTaskDelay(200 / portTICK_PERIOD_MS);

    sendFrame(0x06, 0x00, 25);              // Set volume to 25 (max 30)
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "DFPlayer initialized. Volume=25.");
    initialized = true;
    return true;
#endif
}

// ---------------------------------------------------------------------------
// speak(AudioTrack) — play a specific track number
// ---------------------------------------------------------------------------
void AudioHandler::speak(AudioTrack track) {
    if (track == TRACK_NONE) return;
    logResponse(trackToMessage(track));
#if !ENABLE_DFPLAYER
    return;
#endif
    if (!initialized) {
        ESP_LOGW(TAG, "DFPlayer not ready. Track: %d", (int)track);
        return;
    }
    ESP_LOGI(TAG, "Playing track %d", (int)track);
    sendFrame(0x03, 0x00, (uint8_t)track);  // CMD 0x03: play track N
    vTaskDelay(300 / portTICK_PERIOD_MS);   // allow DFPlayer to begin

    // Wait for BUSY to go LOW (DFPlayer started playing); timeout 1 s
    uint32_t t = esp_timer_get_time() / 1000;
    while (gpio_get_level((gpio_num_t)DFPLAYER_BUSY_PIN) == 1) {
        if ((esp_timer_get_time() / 1000 - t) > 1000) break;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    // Wait for BUSY to go HIGH (playback complete); timeout 30 s
    t = esp_timer_get_time() / 1000;
    while (gpio_get_level((gpio_num_t)DFPLAYER_BUSY_PIN) == 0) {
        if ((esp_timer_get_time() / 1000 - t) > 30000) break;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ---------------------------------------------------------------------------
// speak(const char*) — maps a message string to its track, then plays it
// ---------------------------------------------------------------------------
void AudioHandler::speak(const char* message) {
    AudioTrack track = messageToTrack(message);
    if (track != TRACK_NONE) {
        speak(track);
    } else {
        logResponse(message);
        ESP_LOGI(TAG, "Serial-only reply for unmapped message.");
    }
}

void AudioHandler::playConfirmation() { speak(TRACK_ACTION_CONFIRMED); }
void AudioHandler::playError()        { speak(TRACK_CMD_UNKNOWN);      }

// ---------------------------------------------------------------------------
// messageToTrack — maps caller-supplied message strings to AudioTrack values
// ---------------------------------------------------------------------------
AudioTrack AudioHandler::messageToTrack(const char* message) {
    if (!message) return TRACK_NONE;

    if (strstr(message, "Listening for secret")  ||
        strstr(message, "listening for secret"))     return TRACK_LISTENING_CODE;
    if (strstr(message, "unlocked")              ||
        strstr(message, "Unlocked"))                 return TRACK_UNLOCKED;
    if (strstr(message, "Wrong code")            ||
        strstr(message, "wrong code"))               return TRACK_WRONG_CODE;
    if (strstr(message, "Time expired"))             return TRACK_AUTH_TIMEOUT;
    if (strstr(message, "System locked"))            return TRACK_LOCKED;
    if (strstr(message, "Listening for command") ||
        strstr(message, "listening for command"))    return TRACK_LISTENING_CMD;
    if (strstr(message, "did not understand"))       return TRACK_CMD_UNKNOWN;
    if (strstr(message, "Light turned on"))          return TRACK_LIGHT_TURNING_ON;
    if (strstr(message, "Turning off the light"))    return TRACK_LIGHT_TURNING_OFF;
    if (strstr(message, "already bright")        ||
        strstr(message, "already on"))               return TRACK_BRIGHT;
    if (strstr(message, "Light remains off"))        return TRACK_LIGHT_REMAINS_OFF;
    if (strstr(message, "Fan turned on"))            return TRACK_FAN_TURNING_ON;
    if (strstr(message, "Turning off the fan"))      return TRACK_FAN_TURNING_OFF;
    if (strstr(message, "Temperature is low"))       return TRACK_LOW_TEMP_HUM;
    if (strstr(message, "Fan remains off"))          return TRACK_FAN_REMAINS_OFF;
    if (strstr(message, "No motion"))                return TRACK_NO_MOTION;
    if (strstr(message, "Low voltage")           ||
        strstr(message, "low voltage"))              return TRACK_LOW_VOLTAGE;
    if (strstr(message, "Voltage fluctuation")   ||
        strstr(message, "voltage fluctuation"))      return TRACK_VOLT_FLUCTUATION;
    if (strstr(message, "system status"))            return TRACK_STATUS_INTRO;
    if (strstr(message, "Light is currently on"))    return TRACK_STATUS_LIGHT_ON;
    if (strstr(message, "Light is currently off"))   return TRACK_STATUS_LIGHT_OFF;
    if (strstr(message, "Fan is currently on"))      return TRACK_STATUS_FAN_ON;
    if (strstr(message, "Fan is currently off"))     return TRACK_STATUS_FAN_OFF;
    if (strstr(message, "Motion detected"))          return TRACK_MOTION_DETECTED;
    if (strstr(message, "No motion detected"))       return TRACK_STATUS_MOTION_UPD;
    if (strstr(message, "Do you still want"))        return TRACK_ASK_YES_NO;
    if (strstr(message, "System ready"))             return TRACK_SYSTEM_READY;
    if (strstr(message, "Too many")              ||
        strstr(message, "wait 30"))                  return TRACK_AUTH_LOCKOUT;

    return TRACK_NONE;
}

const char* AudioHandler::trackToMessage(AudioTrack track) {
    switch (track) {
        case TRACK_LISTENING_CODE:    return "Listening for secret code.";
        case TRACK_UNLOCKED:          return "System unlocked.";
        case TRACK_WRONG_CODE:        return "Wrong code. Try again.";
        case TRACK_AUTH_TIMEOUT:      return "Time expired. System locked.";
        case TRACK_LOCKED:            return "System locked successfully.";
        case TRACK_SYSTEM_LOCKED_MSG: return "System locked.";
        case TRACK_LISTENING_CMD:     return "Listening for command.";
        case TRACK_CMD_RECEIVED:      return "Command received.";
        case TRACK_CMD_UNKNOWN:       return "Sorry, I did not understand. Please repeat.";
        case TRACK_PROCESSING:        return "Processing command.";
        case TRACK_LIGHT_TURNING_ON:  return "Light turned on successfully.";
        case TRACK_LIGHT_TURNING_OFF: return "Turning off the light.";
        case TRACK_LIGHT_ALREADY_ON:  return "Light is already on.";
        case TRACK_LIGHT_ALREADY_OFF: return "Light is already off.";
        case TRACK_BRIGHT:            return "It is already bright. Do you still want to turn on the light?";
        case TRACK_LIGHT_ON:          return "Light is on.";
        case TRACK_LIGHT_REMAINS_OFF: return "Light remains off.";
        case TRACK_FAN_TURNING_ON:    return "Fan turned on successfully.";
        case TRACK_FAN_TURNING_OFF:   return "Turning off the fan.";
        case TRACK_FAN_ALREADY_ON:    return "Fan is already on.";
        case TRACK_FAN_ALREADY_OFF:   return "Fan is already off.";
        case TRACK_LOW_TEMP_HUM:      return "Temperature or humidity is low.";
        case TRACK_FAN_ON:            return "Fan is on.";
        case TRACK_FAN_REMAINS_OFF:   return "Fan remains off.";
        case TRACK_NO_MOTION:         return "No motion detected.";
        case TRACK_MOTION_DETECTED:   return "Motion detected.";
        case TRACK_CURRENT_NORMAL:    return "Current and voltage are normal.";
        case TRACK_LOW_VOLTAGE:       return "Warning. Low voltage detected.";
        case TRACK_VOLT_FLUCTUATION:  return "Warning. Voltage fluctuation detected.";
        case TRACK_OVERCURRENT:       return "Warning. Overcurrent detected.";
        case TRACK_TEMP_PREFIX:       return "Temperature status updated.";
        case TRACK_TEMP_SUFFIX:       return "Temperature unit: degrees Celsius.";
        case TRACK_HUMIDITY_PREFIX:   return "Humidity status updated.";
        case TRACK_HUMIDITY_SUFFIX:   return "Humidity unit: percent.";
        case TRACK_STATUS_INTRO:      return "System status.";
        case TRACK_STATUS_LIGHT_ON:   return "Light is currently on.";
        case TRACK_STATUS_LIGHT_OFF:  return "Light is currently off.";
        case TRACK_STATUS_FAN_ON:     return "Fan is currently on.";
        case TRACK_STATUS_FAN_OFF:    return "Fan is currently off.";
        case TRACK_STATUS_MOTION_UPD: return "No motion detected.";
        case TRACK_ASK_YES_NO:        return "Do you still want to continue?";
        case TRACK_ACTION_CONFIRMED:  return "Action confirmed.";
        case TRACK_ACTION_CANCELLED:  return "Action cancelled.";
        case TRACK_SYSTEM_READY:      return "System ready. Say 'hi esp' to wake up.";
        case TRACK_SLEEP_MODE:        return "System is in sleep mode.";
        case TRACK_MIC_ACTIVATED:     return "Microphone activated.";
        case TRACK_GOODBYE:           return "Goodbye.";
        case TRACK_EDGE_ACTIVE:       return "Edge processing active.";
        case TRACK_ALL_OK:            return "All systems normal.";
        case TRACK_AUTH_LOCKOUT:      return "Too many wrong attempts. Please wait 30 seconds.";
        case TRACK_NONE:
        default:                      return nullptr;
    }
}
