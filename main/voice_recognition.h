/*
 * Voice Recognition Interface
 */

#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include <stdint.h>
#include <string>

extern "C" {
#include "model_path.h"
#include "esp_mn_iface.h"
}

class SensorHandler;

class VoiceRecognition {
public:
    VoiceRecognition();
    ~VoiceRecognition();

    bool init(SensorHandler* sensors);
    bool isReady() const;
    std::string pollRecognizedPhrase();

    bool detectSound();
    int getLastLevel() const;
    int getNoiseFloor() const;
    int getPeakToPeak() const;
    int getDynamicThreshold() const;
    int getActiveFrames() const;
    bool isCalibrating() const;
    bool detectWakeWord();
    std::string recognizeCommand();
    std::string recognizeSecretCode();
    bool verifySecretCode(const std::string& code);
    std::string recognizeYesNo();

private:
    bool initialized;
    SensorHandler* sensorHandler;
    srmodel_list_t *models;
    esp_mn_iface_t *multinet;
    model_iface_data_t *modelData;
    int audioChunkSamples;
    int32_t *rawAudioBuffer;
    int16_t *commandBuffer;
    bool soundDetected;
    int lastLevel;
    int lastPeakToPeak;
    int activeSoundFrames;
    int quietSoundFrames;

    void configureI2S();
    bool configureCommands();
};

#endif // VOICE_RECOGNITION_H
