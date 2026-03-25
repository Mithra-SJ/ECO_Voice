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

    // Configure I2S for INMP441 microphone
    configureI2S();

    // Load model file list
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

    sr_handle.wn_model = sr_handle.wn_iface->create(wn_model_name, DET_MODE_3CH_95);
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

    esp_srmodel_deinit(models);

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
    esp_mn_commands_add(5, "eco lock");
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
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
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

bool VoiceRecognition::detectWakeWord() {
    if (!initialized || !sr_handle.wn_model) return false;

    // Read audio from I2S
    size_t bytes_read;
    esp_err_t ret = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytes_read, 100 / portTICK_PERIOD_MS);
    
    if (ret != ESP_OK) {
        ESP_LOGE("VOICE", "I2S read failed: %s", esp_err_to_name(ret));
        return false;
    }

    if (bytes_read < sizeof(audioBuffer)) {
        return false;
    }

    // Detect wake word
    int audio_chp = sr_handle.wn_iface->detect(sr_handle.wn_model, audioBuffer);
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
        size_t bytes_read;
        esp_err_t ret = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer),
                                 &bytes_read, pdMS_TO_TICKS(100));
        if (ret == ESP_OK && bytes_read >= sizeof(audioBuffer)) {
            int command_id = sr_handle.mn_iface->detect(sr_handle.mn_model, audioBuffer);
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
        size_t bytes_read;
        esp_err_t ret = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer),
                                 &bytes_read, pdMS_TO_TICKS(100));
        if (ret == ESP_OK && bytes_read >= sizeof(audioBuffer)) {
            int cmd_id = sr_handle.mn_iface->detect(sr_handle.mn_model, audioBuffer);
            if (cmd_id == SECRET_CODE_CMD_ID) {
                ESP_LOGI("VOICE", "Secret code recognised.");
                return std::string(SECRET_CODE);
            } else if (cmd_id >= 0) {
                // Something was spoken but it was not the secret code
                ESP_LOGI("VOICE", "Wrong speech input for secret code (cmd_id=%d).", cmd_id);
                return "WRONG";
            }
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
        size_t bytes_read;
        esp_err_t ret = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer),
                                 &bytes_read, pdMS_TO_TICKS(100));
        if (ret == ESP_OK && bytes_read >= sizeof(audioBuffer)) {
            int response = sr_handle.mn_iface->detect(sr_handle.mn_model, audioBuffer);
            if (response == 6) return "YES";
            if (response == 7) return "NO";
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    ESP_LOGW("VOICE", "Yes/No timeout — defaulting to NO");
    return "NO";
}



