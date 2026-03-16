/*
 * Voice Recognition Handler
 * Offline wake word and command detection using ESP32-S3 AI acceleration
 */

#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include <Arduino.h>
#include <driver/i2s.h>

class VoiceRecognition {
public:
    VoiceRecognition();
    bool init();
    bool detectWakeWord();
    String recognizeCommand();
    String recognizeSecretCode();
    bool verifySecretCode(String code);
    String recognizeYesNo();

private:
    bool initialized;
    int16_t audioBuffer[512];

    void configureI2S();
    void readAudioSample();
    String processAudio();
    float calculateEnergy();
    bool detectKeyword(const char* keyword);

    // Simple pattern matching for demo
    // In production, use ESP-SR or TensorFlow Lite
    bool matchPattern(const char* pattern);
};

#endif // VOICE_RECOGNITION_H
