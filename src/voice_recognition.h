/*
 * Voice Recognition Handler - ESP-SR Integrated
 * 
 * Uses ESP-SR for offline voice recognition
 * Wake word: "hi esp" (built-in ESP-SR model)
 * Commands: light on/off, fan on/off, status, lock, yes, no
 */

#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include <string>
#include "esp_wn_iface.h"
#include "esp_mn_iface.h"
#include "model_path.h"
#include "esp_mn_speech_commands.h"

// Forward declaration
class SensorHandler;

typedef struct {
    model_iface_data_t *wn_model;          // Wake-net model
    model_iface_data_t *mn_model;          // MultiNet model
    esp_wn_iface_t *wn_iface;
    esp_mn_iface_t *mn_iface;
} sr_handle_t;

class VoiceRecognition {
public:
    VoiceRecognition();
    ~VoiceRecognition();

    bool init(SensorHandler* sensors);
    bool detectWakeWord();
    std::string recognizeCommand();
    std::string recognizeSecretCode();
    bool verifySecretCode(const std::string& code);
    std::string recognizeYesNo();

private:
    bool initialized;
    SensorHandler* sensorHandler;
    sr_handle_t sr_handle;

    // Audio buffer
    int16_t audioBuffer[16000]; // 1 second at 16kHz
    
    void configureI2S();
};

#endif // VOICE_RECOGNITION_H
