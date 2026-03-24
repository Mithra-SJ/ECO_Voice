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
#include "driver/uart.h"
#include "esp_heap_caps.h"
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

    // Configure UART for secret code input (USB serial)
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

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

    ESP_LOGI("VOICE", "Listening for command...");
    
    // Read audio
    size_t bytes_read;
    esp_err_t ret = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytes_read, 100 / portTICK_PERIOD_MS);
    
    if (ret != ESP_OK || bytes_read < sizeof(audioBuffer)) {
        return "";
    }

    // Recognize command
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
    return "";
}

std::string VoiceRecognition::recognizeSecretCode() {
    // Use serial for secret code (fallback for now)
    ESP_LOGI("VOICE", "Enter secret code:");
    return readSerialInput(30000);
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

    ESP_LOGI("VOICE", "Say yes or no...");
    
    size_t bytes_read;
    esp_err_t ret = i2s_read(I2S_NUM_0, audioBuffer, sizeof(audioBuffer), &bytes_read, 100 / portTICK_PERIOD_MS);
    
    if (ret != ESP_OK || bytes_read < sizeof(audioBuffer)) {
        return "NO";
    }

    int response = sr_handle.mn_iface->detect(sr_handle.mn_model, audioBuffer);
    if (response == 6) return "YES";
    if (response == 7) return "NO";
    return "NO";
}

std::string VoiceRecognition::readSerialInput(unsigned long timeoutMs) {
    ESP_LOGI("VOICE", "> ");
    uint32_t startTime = esp_timer_get_time() / 1000;
    std::string input = "";

    while ((esp_timer_get_time() / 1000 - startTime) < timeoutMs) {
        if (sensorHandler) sensorHandler->update();

        uint8_t data;
        int len = uart_read_bytes(UART_NUM_0, &data, 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            if (data == '\n' || data == '\r') {
                if (!input.empty()) {
                    ESP_LOGI("VOICE", "Secret code entry received: %s", input.c_str());
                    return input;
                }
            } else if (data >= 32 && data <= 126) {
                input.push_back((char)data);
                uart_write_bytes(UART_NUM_0, (const char*)&data, 1); // echo back
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    ESP_LOGW("VOICE", "Secret code entry timeout");
    return input;
}


