/*
 * voice_handler.cpp — ECO Voice Online Version
 * ESP-SR WakeNet + MultiNet pipeline running as a FreeRTOS task.
 *
 * On wake word ("hi esp"):
 *   - Opens 10s command window
 *   - Recognized command → toggles relay + writes to Firebase commands node
 *   - Web dashboard stays in sync via the same commands node the buttons use
 */

#include "voice_handler.h"
#include "config.h"
#include "esp_wn_models.h"
#include "esp_mn_models.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

static const char* TAG = "VOICE";

// ── Command IDs — must match esp_mn_commands_add() order below ────────────
#define CMD_LIGHT_ON   0
#define CMD_LIGHT_OFF  1
#define CMD_FAN_ON     2
#define CMD_FAN_OFF    3

VoiceHandler::VoiceHandler()
    : _appliances(nullptr), _firebase(nullptr), initialized(false) {
    memset(&sr, 0, sizeof(sr_handle_t));
}

// ── I2S setup for INMP441 ─────────────────────────────────────────────────
void VoiceHandler::configureI2S() {
    i2s_config_t i2s_cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = 16000,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 1024,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };
    i2s_pin_config_t pin_cfg = {
        .bck_io_num   = I2S_SCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD_PIN
    };
    i2s_driver_install(I2S_NUM_0, &i2s_cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_cfg);
    i2s_zero_dma_buffer(I2S_NUM_0);
    ESP_LOGI(TAG, "I2S ready — INMP441 on SCK=%d WS=%d SD=%d", I2S_SCK_PIN, I2S_WS_PIN, I2S_SD_PIN);
}

// ── ESP-SR init ───────────────────────────────────────────────────────────
bool VoiceHandler::init(ApplianceControl* appliances, FirebaseHandler* firebase) {
    _appliances = appliances;
    _firebase   = firebase;

    configureI2S();

    // Load model list from the 'model' SPIFFS partition (partitions.csv)
    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(TAG, "Failed to load model list — check 'model' partition is flashed");
        return false;
    }

    // WakeNet
    char *wn_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    if (!wn_name) {
        ESP_LOGE(TAG, "No WakeNet model found in partition");
        esp_srmodel_deinit(models);
        return false;
    }
    sr.wn_iface = (esp_wn_iface_t *)esp_wn_handle_from_name(wn_name);
    sr.wn_model = sr.wn_iface->create(wn_name, DET_MODE_3CH_95);
    if (!sr.wn_model) {
        ESP_LOGE(TAG, "WakeNet model create failed");
        esp_srmodel_deinit(models);
        return false;
    }
    ESP_LOGI(TAG, "WakeNet loaded: %s", wn_name);

    // MultiNet
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, NULL);
    if (!mn_name) {
        ESP_LOGE(TAG, "No MultiNet model found in partition");
        esp_srmodel_deinit(models);
        return false;
    }
    sr.mn_iface = esp_mn_handle_from_name(mn_name);
    sr.mn_model = sr.mn_iface->create(mn_name, 6000);
    if (!sr.mn_model) {
        ESP_LOGE(TAG, "MultiNet model create failed");
        esp_srmodel_deinit(models);
        return false;
    }
    ESP_LOGI(TAG, "MultiNet loaded: %s", mn_name);

    esp_srmodel_deinit(models);

    // Register commands
    esp_err_t err = esp_mn_commands_alloc(sr.mn_iface, sr.mn_model);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Commands alloc failed: %s", esp_err_to_name(err));
        return false;
    }
    esp_mn_commands_add(CMD_LIGHT_ON,  "light on");
    esp_mn_commands_add(CMD_LIGHT_OFF, "light off");
    esp_mn_commands_add(CMD_FAN_ON,    "fan on");
    esp_mn_commands_add(CMD_FAN_OFF,   "fan off");
    esp_mn_error_t *mn_err = esp_mn_commands_update();
    if (mn_err && mn_err->num > 0) {
        ESP_LOGW(TAG, "%d command phrase(s) failed to register", mn_err->num);
    }

    ESP_LOGI(TAG, "Voice ready. Say 'hi esp' to wake.");
    initialized = true;
    return true;
}

// ── Start FreeRTOS task on core 1 (core 0 = WiFi/BT) ─────────────────────
void VoiceHandler::startTask() {
    if (!initialized) return;
    xTaskCreatePinnedToCore(voiceTask, "voice_task", 8192, this, 5, NULL, 1);
    ESP_LOGI(TAG, "Voice task started on core 1");
}

void VoiceHandler::voiceTask(void* param) {
    static_cast<VoiceHandler*>(param)->runPipeline();
}

// ── Main audio pipeline loop ──────────────────────────────────────────────
void VoiceHandler::runPipeline() {
    size_t bytes_read;

    while (true) {
        // ── Phase 1: Wait for wake word ──────────────────────────────────
        esp_err_t ret = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer),
                                 &bytes_read, pdMS_TO_TICKS(100));
        if (ret != ESP_OK || bytes_read < sizeof(audioBuffer)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int wn_result = sr.wn_iface->detect(sr.wn_model, audioBuffer);
        if (wn_result <= 0) continue;

        // ── Wake word detected ────────────────────────────────────────────
        Serial.println("[VOICE] Wake word detected! Listening for command...");

        // ── Phase 2: Command window (VOICE_COMMAND_TIMEOUT_MS) ───────────
        uint32_t start_ms = (uint32_t)(esp_timer_get_time() / 1000);
        bool command_found = false;

        while (((uint32_t)(esp_timer_get_time() / 1000) - start_ms) < VOICE_COMMAND_TIMEOUT_MS) {
            ret = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer),
                           &bytes_read, pdMS_TO_TICKS(100));
            if (ret != ESP_OK || bytes_read < sizeof(audioBuffer)) continue;

            int cmd_id = sr.mn_iface->detect(sr.mn_model, audioBuffer);
            if (cmd_id >= 0) {
                handleCommand(cmd_id);
                command_found = true;
                break;
            }
        }

        if (!command_found) {
            Serial.println("[VOICE] Command timeout. Back to wake word mode.");
        }
    }
}

// ── Execute command + sync to Firebase ───────────────────────────────────
void VoiceHandler::handleCommand(int cmdId) {
    switch (cmdId) {
        case CMD_LIGHT_ON:
            Serial.println("[VOICE] Command: Light ON");
            _appliances->setLight(true);
            _firebase->writeCommand("light", true);
            break;
        case CMD_LIGHT_OFF:
            Serial.println("[VOICE] Command: Light OFF");
            _appliances->setLight(false);
            _firebase->writeCommand("light", false);
            break;
        case CMD_FAN_ON:
            Serial.println("[VOICE] Command: Fan ON");
            _appliances->setFan(true);
            _firebase->writeCommand("fan", true);
            break;
        case CMD_FAN_OFF:
            Serial.println("[VOICE] Command: Fan OFF");
            _appliances->setFan(false);
            _firebase->writeCommand("fan", false);
            break;
        default:
            Serial.printf("[VOICE] Unhandled command ID: %d\n", cmdId);
            break;
    }
}
