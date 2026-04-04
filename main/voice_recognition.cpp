/*
 * Voice Recognition Implementation
 */

#include "voice_recognition.h"
#include "sensor_handler.h"
#include "config.h"
#include "secrets.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <limits>

#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"

static const char *TAG = "VOICE";

namespace {
struct SpeechCommand {
    int id;
    const char *phrase;
};

constexpr SpeechCommand kSpeechCommands[] = {
    {1, "HI"},
    {2, "HELLO"},
    {3, "LIGHT ON"},
    {4, "LIGHT OFF"},
    {5, "FAN ON"},
    {6, "FAN OFF"},
    {7, "STATUS"},
    {8, "LOCK"},
    {9, "YES"},
    {10, "NO"},
};

std::string normalizePhrase(const char *input) {
    if (input == nullptr) {
        return "";
    }

    std::string phrase;
    phrase.reserve(std::strlen(input));

    bool previousSpace = true;
    for (const char *p = input; *p != '\0'; ++p) {
        unsigned char ch = static_cast<unsigned char>(*p);
        if (std::isspace(ch)) {
            if (!previousSpace) {
                phrase.push_back(' ');
                previousSpace = true;
            }
            continue;
        }

        phrase.push_back(static_cast<char>(std::tolower(ch)));
        previousSpace = false;
    }

    if (!phrase.empty() && phrase.back() == ' ') {
        phrase.pop_back();
    }

    return phrase;
}

std::string uppercasePhrase(const char *input) {
    std::string phrase = normalizePhrase(input);
    std::transform(phrase.begin(), phrase.end(), phrase.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return phrase;
}

const char *lookupCommandText(int commandId) {
    for (const auto &command : kSpeechCommands) {
        if (command.id == commandId) {
            return command.phrase;
        }
    }
    if (commandId == 11) {
        return SECRET_CODE_PHRASE;
    }
    return "";
}

int16_t convertInmp441SampleToS16(int32_t rawSample) {
    // INMP441 provides signed 24-bit samples left-justified in a 32-bit slot.
    int32_t sample24 = rawSample >> 8;
    if ((sample24 & 0x00800000) != 0) {
        sample24 |= ~0x00FFFFFF;
    }
    return static_cast<int16_t>(sample24 >> 8);
}
} // namespace

VoiceRecognition::VoiceRecognition() :
    initialized(false),
    sensorHandler(nullptr),
    models(nullptr),
    multinet(nullptr),
    modelData(nullptr),
    audioChunkSamples(0),
    rawAudioBuffer(nullptr),
    commandBuffer(nullptr),
    soundDetected(false),
    lastLevel(0),
    lastPeakToPeak(0),
    activeSoundFrames(0),
    quietSoundFrames(0) {
}

VoiceRecognition::~VoiceRecognition() {
    if (modelData != nullptr && multinet != nullptr) {
        multinet->destroy(modelData);
        modelData = nullptr;
    }

    esp_mn_commands_free();

    if (models != nullptr) {
        esp_srmodel_deinit(models);
        models = nullptr;
    }

    if (rawAudioBuffer != nullptr) {
        free(rawAudioBuffer);
        rawAudioBuffer = nullptr;
    }

    if (commandBuffer != nullptr) {
        free(commandBuffer);
        commandBuffer = nullptr;
    }

    if (initialized) {
        i2s_driver_uninstall(I2S_PORT);
    }
}

bool VoiceRecognition::init(SensorHandler* sensors) {
    sensorHandler = sensors;

    configureI2S();

    models = esp_srmodel_init("model");
    if (models == nullptr) {
        ESP_LOGE(TAG, "Failed to load ESP-SR models from the 'model' partition.");
        return false;
    }

    char *modelName = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    if (modelName == nullptr) {
        ESP_LOGE(TAG, "No English MultiNet model found in the model partition.");
        return false;
    }

    multinet = esp_mn_handle_from_name(modelName);
    if (multinet == nullptr) {
        ESP_LOGE(TAG, "Failed to get MultiNet handle for model %s.", modelName);
        return false;
    }

    modelData = multinet->create(modelName, 4000);
    if (modelData == nullptr) {
        ESP_LOGE(TAG, "Failed to create MultiNet model instance.");
        return false;
    }

    audioChunkSamples = multinet->get_samp_chunksize(modelData);
    if (audioChunkSamples <= 0) {
        ESP_LOGE(TAG, "Invalid MultiNet chunk size: %d", audioChunkSamples);
        return false;
    }

    rawAudioBuffer = static_cast<int32_t *>(malloc(static_cast<size_t>(audioChunkSamples) * sizeof(int32_t)));
    commandBuffer = static_cast<int16_t *>(malloc(static_cast<size_t>(audioChunkSamples) * sizeof(int16_t)));
    if (rawAudioBuffer == nullptr || commandBuffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate audio buffers for speech recognition.");
        return false;
    }

    if (!configureCommands()) {
        return false;
    }

    multinet->set_det_threshold(modelData, 0.05f);
    initialized = true;

    ESP_LOGI(TAG, "ESP-SR ready with %d predefined commands.", static_cast<int>(sizeof(kSpeechCommands) / sizeof(kSpeechCommands[0])));
    return true;
}

bool VoiceRecognition::isReady() const {
    return initialized;
}

bool VoiceRecognition::configureCommands() {
    if (esp_mn_commands_alloc(multinet, modelData) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate MultiNet command table.");
        return false;
    }

    esp_mn_commands_clear();

    for (const auto &command : kSpeechCommands) {
        if (esp_mn_commands_add(command.id, command.phrase) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add speech command: %s", command.phrase);
            return false;
        }
    }

    std::string secretPhrase = uppercasePhrase(SECRET_CODE_PHRASE);
    if (esp_mn_commands_add(11, secretPhrase.c_str()) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add secret code phrase: %s", SECRET_CODE_PHRASE);
        return false;
    }

    esp_mn_error_t *errors = esp_mn_commands_update();
    if (errors != nullptr) {
        ESP_LOGE(TAG, "One or more speech commands could not be parsed by MultiNet.");
        return false;
    }

    multinet->print_active_speech_commands(modelData);
    return true;
}

void VoiceRecognition::configureI2S() {
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2s_config.sample_rate = SAMPLE_RATE;
    i2s_config.bits_per_sample = BITS_PER_SAMPLE;
    i2s_config.channel_format = I2S_CHANNEL;
    i2s_config.communication_format = I2S_COMM_FORMAT_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 8;
    i2s_config.dma_buf_len = 256;
    i2s_config.use_apll = false;
    i2s_config.tx_desc_auto_clear = false;
    i2s_config.fixed_mclk = 0;

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = I2S_SCK_PIN;
    pin_config.ws_io_num = I2S_WS_PIN;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num = I2S_SD_PIN;

    gpio_reset_pin((gpio_num_t)I2S_SD_PIN);
    gpio_set_direction((gpio_num_t)I2S_SD_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)I2S_SD_PIN, GPIO_FLOATING);

    i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);
}

std::string VoiceRecognition::pollRecognizedPhrase() {
    if (!initialized) {
        return "";
    }

    const size_t channelCount = 2;
    const size_t targetBytes = static_cast<size_t>(audioChunkSamples) * channelCount * sizeof(int32_t);
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_PORT, rawAudioBuffer, targetBytes, &bytesRead, pdMS_TO_TICKS(120));
    if (err != ESP_OK || bytesRead < targetBytes) {
        return "";
    }

    int64_t leftEnergy = 0;
    int64_t rightEnergy = 0;
    for (int i = 0; i < audioChunkSamples; ++i) {
        const int32_t leftRaw = rawAudioBuffer[i * 2];
        const int32_t rightRaw = rawAudioBuffer[i * 2 + 1];
        const int16_t leftSample = convertInmp441SampleToS16(leftRaw);
        const int16_t rightSample = convertInmp441SampleToS16(rightRaw);
        leftEnergy += std::abs(static_cast<int>(leftSample));
        rightEnergy += std::abs(static_cast<int>(rightSample));
    }

    const bool useRightChannel = rightEnergy > leftEnergy;
    int sampleMin = std::numeric_limits<int16_t>::max();
    int sampleMax = std::numeric_limits<int16_t>::min();
    int64_t sampleSum = 0;
    int peakAbs = 1;
    for (int i = 0; i < audioChunkSamples; ++i) {
        const int32_t rawSample = rawAudioBuffer[i * 2 + (useRightChannel ? 1 : 0)];
        const int16_t sample = convertInmp441SampleToS16(rawSample);
        peakAbs = std::max(peakAbs, std::abs(static_cast<int>(sample)));
    }

    const int gain = std::max(1, std::min(MIC_MAX_GAIN, MIC_TARGET_PEAK / peakAbs));
    for (int i = 0; i < audioChunkSamples; ++i) {
        const int32_t rawSample = rawAudioBuffer[i * 2 + (useRightChannel ? 1 : 0)];
        int sample = static_cast<int>(convertInmp441SampleToS16(rawSample)) * gain;
        sample = std::max(static_cast<int>(std::numeric_limits<int16_t>::min()),
                          std::min(static_cast<int>(std::numeric_limits<int16_t>::max()), sample));
        commandBuffer[i] = static_cast<int16_t>(sample);
        sampleSum += std::abs(sample);
        sampleMin = std::min(sampleMin, sample);
        sampleMax = std::max(sampleMax, sample);
    }

    lastLevel = static_cast<int>(sampleSum / std::max(audioChunkSamples, 1));
    lastPeakToPeak = sampleMax - sampleMin;

    const bool chunkHasSound =
        lastLevel >= SOUND_ACTIVITY_THRESHOLD || lastPeakToPeak >= SOUND_MIN_PEAK_TO_PEAK;

    if (chunkHasSound) {
        ++activeSoundFrames;
        quietSoundFrames = 0;
        if (activeSoundFrames >= SOUND_CONSECUTIVE_FRAMES) {
            soundDetected = true;
        }
    } else {
        ++quietSoundFrames;
        activeSoundFrames = 0;
        if (quietSoundFrames >= SOUND_RELEASE_FRAMES) {
            soundDetected = false;
        }
    }

    esp_mn_state_t state = multinet->detect(modelData, commandBuffer);
    if (state == ESP_MN_STATE_DETECTED) {
        esp_mn_results_t *result = multinet->get_results(modelData);
        std::string phrase;

        if (result != nullptr && result->num > 0) {
            if (result->command_id[0] > 0) {
                phrase = normalizePhrase(lookupCommandText(result->command_id[0]));
            }
        }

        multinet->clean(modelData);
        return phrase;
    }

    if (state == ESP_MN_STATE_TIMEOUT) {
        multinet->clean(modelData);
    }

    return "";
}

bool VoiceRecognition::detectSound() {
    return soundDetected;
}

int VoiceRecognition::getLastLevel() const {
    return lastLevel;
}

int VoiceRecognition::getNoiseFloor() const {
    return 0;
}

int VoiceRecognition::getPeakToPeak() const {
    return lastPeakToPeak;
}

int VoiceRecognition::getDynamicThreshold() const {
    return 0;
}

int VoiceRecognition::getActiveFrames() const {
    return activeSoundFrames;
}

bool VoiceRecognition::isCalibrating() const {
    return false;
}

bool VoiceRecognition::detectWakeWord() {
    return false;
}

std::string VoiceRecognition::recognizeCommand() {
    return pollRecognizedPhrase();
}

std::string VoiceRecognition::recognizeSecretCode() {
    return "";
}

bool VoiceRecognition::verifySecretCode(const std::string& code) {
    std::string normalizedCode = normalizePhrase(code.c_str());
    std::string normalizedSecret = normalizePhrase(SECRET_CODE_PHRASE);
    return normalizedCode == normalizedSecret || normalizedCode == normalizePhrase(SECRET_CODE);
}

std::string VoiceRecognition::recognizeYesNo() {
    std::string phrase = pollRecognizedPhrase();
    if (phrase == "yes" || phrase == "no") {
        return phrase;
    }
    return "";
}
