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
#include "esp_afe_sr_models.h"
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
    {1, "hi esp"},
    {2, "hello there"},
    {3, "turn light on"},
    {4, "turn light off"},
    {5, "turn fan on"},
    {6, "turn fan off"},
    {7, "show status"},
    {8, "lock system"},
    {9, "yes please"},
    {10, "no thanks"},
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

bool isKnownRecognizedPhrase(const std::string &phrase) {
    if (phrase.empty()) {
        return false;
    }

    for (const auto &command : kSpeechCommands) {
        if (phrase == normalizePhrase(command.phrase)) {
            return true;
        }
    }

    return phrase == normalizePhrase(SECRET_CODE_PHRASE);
}

std::string extractRecognizedPhrase(const esp_mn_results_t *result) {
    if (result == nullptr || result->num <= 0) {
        return "";
    }

    std::string phrase = normalizePhrase(result->string);
    if (!phrase.empty()) {
        return phrase;
    }

    phrase = normalizePhrase(result->raw_string);
    if (!phrase.empty()) {
        return phrase;
    }

    if (result->command_id[0] > 0) {
        phrase = normalizePhrase(lookupCommandText(result->command_id[0]));
        if (!phrase.empty()) {
            return phrase;
        }
    }

    if (result->phrase_id[0] > 0) {
        phrase = normalizePhrase(lookupCommandText(result->phrase_id[0]));
    }

    return phrase;
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
    afeHandle(nullptr),
    afeData(nullptr),
    multinet(nullptr),
    modelData(nullptr),
    audioChunkSamples(0),
    afeFeedSamples(0),
    rawAudioBuffer(nullptr),
    commandBuffer(nullptr),
    soundDetected(false),
    lastLevel(0),
    lastPeakToPeak(0),
    activeSoundFrames(0),
    quietSoundFrames(0),
    noiseFloorLevel(0),
    calibrationFrames(0),
    dynamicThreshold(SOUND_ACTIVITY_THRESHOLD),
    useSlot0(true),
    calSlot0Energy(0),
    calSlot1Energy(0) {
}

VoiceRecognition::~VoiceRecognition() {
    if (afeData != nullptr && afeHandle != nullptr) {
        afeHandle->destroy(afeData);
        afeData = nullptr;
    }

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

    afe_config_t *afeConfig = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if (afeConfig == nullptr) {
        ESP_LOGE(TAG, "Failed to create AFE config.");
        return false;
    }

    // MultiNet command recognition is used directly without a separate wake-word stage.
    afeConfig->wakenet_init = false;
    afeConfig->vad_init = true;
    afeConfig->agc_init = true;
    afeConfig->fixed_first_channel = true;
    afeConfig->fixed_output_channel = true;
    afeConfig->afe_linear_gain = 1.0f;

    afeHandle = esp_afe_handle_from_config(afeConfig);
    if (afeHandle == nullptr) {
        ESP_LOGE(TAG, "Failed to get AFE handle.");
        afe_config_free(afeConfig);
        return false;
    }

    afeData = afeHandle->create_from_config(afeConfig);
    afe_config_free(afeConfig);
    if (afeData == nullptr) {
        ESP_LOGE(TAG, "Failed to create AFE instance.");
        return false;
    }

    audioChunkSamples = multinet->get_samp_chunksize(modelData);
    if (audioChunkSamples <= 0) {
        ESP_LOGE(TAG, "Invalid MultiNet chunk size: %d", audioChunkSamples);
        return false;
    }

    afeFeedSamples = afeHandle->get_feed_chunksize(afeData);
    const int afeFetchSamples = afeHandle->get_fetch_chunksize(afeData);
    if (afeFeedSamples <= 0 || afeFetchSamples <= 0) {
        ESP_LOGE(TAG, "Invalid AFE chunk sizes. feed=%d fetch=%d", afeFeedSamples, afeFetchSamples);
        return false;
    }

    if (afeFetchSamples != audioChunkSamples) {
        ESP_LOGE(TAG, "AFE fetch chunk (%d) does not match MultiNet chunk (%d).", afeFetchSamples, audioChunkSamples);
        return false;
    }

    // Read stereo I2S slots from INMP441 and collapse to mono for AFE feed.
    rawAudioBuffer = static_cast<int32_t *>(malloc(static_cast<size_t>(afeFeedSamples) * 2 * sizeof(int32_t)));
    commandBuffer = static_cast<int16_t *>(malloc(static_cast<size_t>(afeFeedSamples) * sizeof(int16_t)));
    if (rawAudioBuffer == nullptr || commandBuffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate audio buffers for speech recognition.");
        return false;
    }

    if (!configureCommands()) {
        return false;
    }

    multinet->set_det_threshold(modelData, 0.35f);
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
        const std::string phrase = normalizePhrase(command.phrase);
        if (phrase.length() < ESP_MN_MIN_PHRASE_LEN || multinet->check_speech_command(modelData, phrase.c_str()) != 0) {
            ESP_LOGE(TAG, "Speech command is not valid for the active MultiNet model: %s", command.phrase);
            return false;
        }

        if (esp_mn_commands_add(command.id, const_cast<char *>(phrase.c_str())) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add speech command: %s", command.phrase);
            return false;
        }
    }

    const std::string secretPhrase = normalizePhrase(SECRET_CODE_PHRASE);
    if (secretPhrase.length() < ESP_MN_MIN_PHRASE_LEN ||
        multinet->check_speech_command(modelData, secretPhrase.c_str()) != 0) {
        ESP_LOGE(TAG, "Secret code phrase is not valid for the active MultiNet model: %s", SECRET_CODE_PHRASE);
        return false;
    }

    if (esp_mn_commands_add(11, const_cast<char *>(secretPhrase.c_str())) != ESP_OK) {
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
    const size_t targetBytes = static_cast<size_t>(afeFeedSamples) * channelCount * sizeof(int32_t);
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_PORT, rawAudioBuffer, targetBytes, &bytesRead, pdMS_TO_TICKS(120));
    if (err != ESP_OK || bytesRead < targetBytes) {
        return "";
    }

    if (calibrationFrames < SOUND_CALIBRATION_FRAMES) {
        for (int i = 0; i < afeFeedSamples; ++i) {
            calSlot0Energy += std::abs(static_cast<int>(convertInmp441SampleToS16(rawAudioBuffer[i * 2 + 0])));
            calSlot1Energy += std::abs(static_cast<int>(convertInmp441SampleToS16(rawAudioBuffer[i * 2 + 1])));
        }
        calibrationFrames++;
        if (calibrationFrames == SOUND_CALIBRATION_FRAMES) {
            useSlot0 = (calSlot0Energy >= calSlot1Energy);
        }
        soundDetected = false;
        return "";
    }

    const int activeSlotIndex = useSlot0 ? 0 : 1;
    int peakAbs = 1;
    for (int i = 0; i < afeFeedSamples; ++i) {
        const int16_t sample = convertInmp441SampleToS16(rawAudioBuffer[i * 2 + activeSlotIndex]);
        peakAbs = std::max(peakAbs, std::abs(static_cast<int>(sample)));
    }

    const int gain = std::max(1, std::min(MIC_MAX_GAIN, MIC_TARGET_PEAK / peakAbs));
    for (int i = 0; i < afeFeedSamples; ++i) {
        int sample = static_cast<int>(convertInmp441SampleToS16(rawAudioBuffer[i * 2 + activeSlotIndex])) * gain;
        sample = std::max(static_cast<int>(std::numeric_limits<int16_t>::min()),
                          std::min(static_cast<int>(std::numeric_limits<int16_t>::max()), sample));
        commandBuffer[i] = static_cast<int16_t>(sample);
    }

    if (afeHandle->feed(afeData, commandBuffer) <= 0) {
        return "";
    }

    afe_fetch_result_t *afeResult = afeHandle->fetch_with_delay(afeData, pdMS_TO_TICKS(1));
    if (afeResult == nullptr || afeResult->ret_value != ESP_OK || afeResult->data == nullptr ||
        afeResult->data_size < audioChunkSamples * static_cast<int>(sizeof(int16_t))) {
        return "";
    }

    int sampleMin = std::numeric_limits<int16_t>::max();
    int sampleMax = std::numeric_limits<int16_t>::min();
    int64_t sampleSum = 0;
    for (int i = 0; i < audioChunkSamples; ++i) {
        const int sample = afeResult->data[i];
        sampleSum += std::abs(sample);
        sampleMin = std::min(sampleMin, sample);
        sampleMax = std::max(sampleMax, sample);
    }
    lastLevel = static_cast<int>(sampleSum / std::max(audioChunkSamples, 1));
    lastPeakToPeak = sampleMax - sampleMin;

    if (calibrationFrames < SOUND_CALIBRATION_FRAMES + 20) {
        noiseFloorLevel = (noiseFloorLevel * (calibrationFrames - SOUND_CALIBRATION_FRAMES) + lastLevel)
                          / (calibrationFrames - SOUND_CALIBRATION_FRAMES + 1);
        calibrationFrames++;
        if (calibrationFrames == SOUND_CALIBRATION_FRAMES + 20) {
            dynamicThreshold = std::max(SOUND_ACTIVITY_THRESHOLD, noiseFloorLevel * 6 / 5);
        }
        soundDetected = false;
        return "";
    }

    const bool chunkHasSound = (afeResult->vad_state == VAD_SPEECH) || (lastLevel >= dynamicThreshold);

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

    esp_mn_state_t state = multinet->detect(modelData, afeResult->data);

    if (state == ESP_MN_STATE_DETECTED) {
        esp_mn_results_t *result = multinet->get_results(modelData);
        std::string phrase = extractRecognizedPhrase(result);
        const float probability = (result != nullptr && result->num > 0) ? result->prob[0] : 0.0f;

        if (result != nullptr && (!isKnownRecognizedPhrase(phrase) || probability < VOICE_MIN_RESULT_PROB)) {
            phrase.clear();
        }

        if (phrase.empty() && result != nullptr && probability >= VOICE_MIN_RESULT_PROB) {
            phrase = "recognized unknown word";
        }

        multinet->clean(modelData);
        return phrase;
    }

    if (state == ESP_MN_STATE_TIMEOUT) {
        esp_mn_results_t *result = multinet->get_results(modelData);
        if (result != nullptr && result->num > 0) {
            printf("[MULTINET] TIMEOUT - best candidate: cmd_id=%d prob=%.3f string='%s'\n",
                   result->command_id[0], result->prob[0], result->string);
        } else {
            printf("[MULTINET] TIMEOUT - speech window ended, no command matched\n");
        }
        multinet->clean(modelData);
        return "";
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
    return noiseFloorLevel;
}

int VoiceRecognition::getPeakToPeak() const {
    return lastPeakToPeak;
}

int VoiceRecognition::getDynamicThreshold() const {
    return dynamicThreshold;
}

int VoiceRecognition::getActiveFrames() const {
    return activeSoundFrames;
}

bool VoiceRecognition::isCalibrating() const {
    return calibrationFrames < SOUND_CALIBRATION_FRAMES;
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
    if (phrase.rfind("yes", 0) == 0) {
        return "yes";
    }
    if (phrase.rfind("no", 0) == 0) {
        return "no";
    }
    return "";
}
