/*
 * Voice Recognition Implementation - ESP-SR Integrated
 */

#include "voice_recognition.h"
#include "sensor_handler.h"
#include "config.h"
#include "secrets.h"
#include <string>
#include <cstring>
#include <algorithm>
#include <cctype>
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "model_path.h"
// #include "hiesp.h"
#include "esp_mn_speech_commands.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "esp_timer.h"

VoiceRecognition::VoiceRecognition() : initialized(false), sensorHandler(nullptr) {
    memset(&sr_handle, 0, sizeof(sr_handle_t));
}

VoiceRecognition::~VoiceRecognition() {
    if (sr_handle.wn_model && sr_handle.wn_iface) {
        sr_handle.wn_iface->destroy(sr_handle.wn_model);
    }
    if (sr_handle.mn_model && sr_handle.mn_iface) {
        sr_handle.mn_iface->destroy(sr_handle.mn_model);
    }
}

bool VoiceRecognition::init(SensorHandler* sensors) {
    ESP_LOGI("VOICE", "Initializing ESP-SR Voice Recognition...");
    sensorHandler = sensors;

    // Load models FIRST — esp_srmodel_init needs internal DMA-capable RAM to mount the model
    // partition (FAT). With I2S installed first (dma_buf_len=240, 8 bufs = 15KB of 32KB pool),
    // there is insufficient DMA RAM left and esp_srmodel_filter returns NULL.
    // Models are loaded into PSRAM; after esp_srmodel_deinit() the temporary DMA RAM is freed.
    // I2S is configured last so it gets a full pool to allocate from.
    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE("VOICE", "Failed to load model file list");
        return false;
    }

    // Get wake-net model
    char *wn_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    if (!wn_model_name) {
        ESP_LOGE("VOICE", "No wake-net model found");
        esp_srmodel_deinit(models);
        return false;
    }

    // Initialize wake-net
    sr_handle.wn_iface = (esp_wn_iface_t *)esp_wn_handle_from_name(wn_model_name);
    if (!sr_handle.wn_iface) {
        ESP_LOGE("VOICE", "Failed to get wake-net interface");
        esp_srmodel_deinit(models);
        return false;
    }

    // DET_MODE_2CH_90: correct for single-mic (INMP441 mono-left).
    // DET_MODE_3CH_95 expects 3-channel audio and reads past our buffer — crash risk.
    sr_handle.wn_model = sr_handle.wn_iface->create(wn_model_name, DET_MODE_90);
    if (!sr_handle.wn_model) {
        ESP_LOGE("VOICE", "Failed to create wake-net model");
        esp_srmodel_deinit(models);
        return false;
    }

    // Get multi-net model for command recognition
    char *mn_model_name = esp_srmodel_filter(models, ESP_MN_PREFIX, NULL);
    if (!mn_model_name) {
        ESP_LOGE("VOICE", "No multi-net model found");
        esp_srmodel_deinit(models);
        return false;
    }

    // Initialize multi-net
    sr_handle.mn_iface = esp_mn_handle_from_name(mn_model_name);
    if (!sr_handle.mn_iface) {
        ESP_LOGE("VOICE", "Failed to get multi-net interface");
        esp_srmodel_deinit(models);
        return false;
    }

    sr_handle.mn_model = sr_handle.mn_iface->create(mn_model_name, 6000);
    if (!sr_handle.mn_model) {
        ESP_LOGE("VOICE", "Failed to create multi-net model");
        esp_srmodel_deinit(models);
        return false;
    }

    esp_srmodel_deinit(models);  // Releases temporary DMA-capable RAM used during model scan

    // Configure I2S now — DMA pool is free after esp_srmodel_deinit
    configureI2S();

    // Register speech commands with MultiNet
    // Command IDs must match the switch-case in recognizeCommand() and SECRET_CODE_CMD_ID in config.h
    esp_err_t cmds_err = esp_mn_commands_alloc(sr_handle.mn_iface, sr_handle.mn_model);
    if (cmds_err != ESP_OK) {
        ESP_LOGE("VOICE", "esp_mn_commands_alloc failed — cannot register commands.");
        return false;
    }
    esp_mn_commands_add(0, "light on");
    esp_mn_commands_add(1, "light off");
    esp_mn_commands_add(2, "fan on");
    esp_mn_commands_add(3, "fan off");
    esp_mn_commands_add(4, "status");
    esp_mn_commands_add(5, "lock");
    esp_mn_commands_add(6, "yes");
    esp_mn_commands_add(7, "no");
    esp_mn_commands_add(SECRET_CODE_CMD_ID, SECRET_CODE_PHRASE);  // secret code phrase from secrets.h
    esp_mn_error_t *mn_err = esp_mn_commands_update();
    if (mn_err && mn_err->num > 0) {
        ESP_LOGW("VOICE", "MultiNet: %d command phrase(s) failed to register.", mn_err->num);
    }
    ESP_LOGI("VOICE", "MultiNet commands registered (secret code phrase: \"%s\").", SECRET_CODE_PHRASE);

    ESP_LOGI("VOICE", "ESP-SR initialized successfully.");
    initialized = true;
    return true;
}

void VoiceRecognition::configureI2S() {
    // I2S configuration for INMP441
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 240,   // 240 samples × 4 bytes = 960 bytes/buf; 2 bufs fill one 480-sample WakeNet chunk
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

bool VoiceRecognition::readAudioChunk() {
    // Accumulate exactly sizeof(audioBuffer) bytes (1920 bytes = 480 × int32_t).
    // dma_buf_len=240 means each i2s_read may return only 960 bytes — loop until full.
    size_t total = 0;
    uint8_t *buf = (uint8_t *)audioBuffer;
    const size_t needed = sizeof(audioBuffer);
    const uint32_t deadline_ms = 300;
    uint32_t start = esp_timer_get_time() / 1000;
    while (total < needed) {
        size_t br = 0;
        esp_err_t r = i2s_read(I2S_NUM_0, buf + total, needed - total, &br, pdMS_TO_TICKS(50));
        if (r != ESP_OK) return false;
        total += br;
        if ((esp_timer_get_time() / 1000 - start) > deadline_ms) return false;
    }
    // Convert 32-bit INMP441 (Philips I2S: 24-bit audio at bits[30:7]) → 16-bit PCM.
    // Shift >>11 for maximum sensitivity (common in Espressif ESP-SR examples).
    // Forward iteration is safe: pcm16[i] at byte 2i overwrites within already-read audioBuffer[i/2].
    int16_t *pcm16 = (int16_t *)audioBuffer;
    int32_t max_amp = 0;
    for (int i = 0; i < 480; i++) {
        int32_t raw = audioBuffer[i];
        int32_t abs_val = raw < 0 ? -raw : raw;
        if (abs_val > max_amp) max_amp = abs_val;
        pcm16[i] = (int16_t)(raw >> 11);
    }

    // Print amplitude every ~100 chunks (~3s) so we can confirm mic is alive
    static int dbg_count = 0;
    if (++dbg_count >= 100) {
        dbg_count = 0;
        ESP_LOGI("MIC_DBG", "max_amp_raw=%ld  (should be >100000 when speaking)", (long)max_amp);
    }
    return true;
}

bool VoiceRecognition::detectWakeWord() {
    if (!initialized || !sr_handle.wn_model) return false;

    if (!readAudioChunk()) return false;

    int16_t *pcm16 = (int16_t *)audioBuffer;
    int audio_chp = sr_handle.wn_iface->detect(sr_handle.wn_model, pcm16);
    if (audio_chp > 0) {
        ESP_LOGI("VOICE", "Wake word detected!");
        return true;
    }
    return false;
}

std::string VoiceRecognition::recognizeCommand() {
    if (!initialized || !sr_handle.mn_model) return "";

    ESP_LOGI("VOICE", "Listening for command (timeout=%dms)...", COMMAND_TIMEOUT_MS);

    uint32_t start = esp_timer_get_time() / 1000;
    while ((esp_timer_get_time() / 1000 - start) < COMMAND_TIMEOUT_MS) {
        if (!readAudioChunk()) continue;
        {
            int16_t *pcm16 = (int16_t *)audioBuffer;
            int command_id = sr_handle.mn_iface->detect(sr_handle.mn_model, pcm16);
            if (command_id >= 0) {
                switch (command_id) {
                    case 0: return "LIGHT_ON";
                    case 1: return "LIGHT_OFF";
                    case 2: return "FAN_ON";
                    case 3: return "FAN_OFF";
                    case 4: return "STATUS";
                    case 5: return "LOCK";
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    return "";  // nothing heard within COMMAND_TIMEOUT_MS
}

std::string VoiceRecognition::recognizeSecretCode() {
    if (!initialized || !sr_handle.mn_model) return "";

    // Listen for up to 3 seconds for the secret code phrase (SECRET_CODE_CMD_ID = 8).
    // Returns SECRET_CODE string if the phrase is recognised.
    // Returns "WRONG" if any other command is heard (counted as a bad attempt by caller).
    // Returns ""      if no speech detected in this window (caller loops again).
    ESP_LOGI("VOICE", "Listening for secret code (cmd_id=%d, phrase=\"%s\")...",
             SECRET_CODE_CMD_ID, SECRET_CODE_PHRASE);

    uint32_t start = esp_timer_get_time() / 1000;
    while ((esp_timer_get_time() / 1000 - start) < 3000) {
        if (!readAudioChunk()) continue;
        int16_t *pcm16 = (int16_t *)audioBuffer;
        int cmd_id = sr_handle.mn_iface->detect(sr_handle.mn_model, pcm16);
        if (cmd_id == SECRET_CODE_CMD_ID) {
            ESP_LOGI("VOICE", "Secret code recognised.");
            return std::string(SECRET_CODE);
        } else if (cmd_id >= 0) {
            ESP_LOGI("VOICE", "Wrong speech input for secret code (cmd_id=%d).", cmd_id);
            return "WRONG";
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    return "";  // nothing heard this window — caller will loop within 30-second auth window
}

bool VoiceRecognition::verifySecretCode(const std::string& code) {
    std::string input = code;
    std::transform(input.begin(), input.end(), input.begin(), ::tolower);
    
    std::string secret = std::string(SECRET_CODE);
    std::transform(secret.begin(), secret.end(), secret.begin(), ::tolower);
    
    return (input == secret || input.find(secret) != std::string::npos);
}

std::string VoiceRecognition::recognizeYesNo() {
    if (!initialized || !sr_handle.mn_model) return "NO";

    ESP_LOGI("VOICE", "Say yes or no (timeout=%dms)...", YES_NO_TIMEOUT_MS);

    uint32_t start = esp_timer_get_time() / 1000;
    while ((esp_timer_get_time() / 1000 - start) < YES_NO_TIMEOUT_MS) {
        if (!readAudioChunk()) continue;
        int16_t *pcm16 = (int16_t *)audioBuffer;
        int response = sr_handle.mn_iface->detect(sr_handle.mn_model, pcm16);
        if (response == 6) return "YES";
        if (response == 7) return "NO";
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    ESP_LOGW("VOICE", "Yes/No timeout — defaulting to NO");
    return "NO";
}



