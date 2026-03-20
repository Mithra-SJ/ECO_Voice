/*
 * Voice Recognition Handler
 * Offline wake word and command detection using ESP32-S3 AI acceleration.
 *
 * Current mode: DEMO — Serial input used for testing.
 * Production:   Uncomment production blocks and integrate ESP-SR (WakeNet + MultiNet).
 *               See ESP_SR_INTEGRATION.md for full integration guide.
 */

#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include <Arduino.h>
#include <driver/i2s.h>

// Forward declaration — avoids circular dependency
class SensorHandler;

class VoiceRecognition {
public:
    VoiceRecognition();

    // Pass SensorHandler so sensor readings stay fresh during blocking loops
    bool init(SensorHandler* sensors);

    bool detectWakeWord();
    String recognizeCommand();
    String recognizeSecretCode();
    bool verifySecretCode(const String& code);
    String recognizeYesNo();

private:
    bool initialized;
    SensorHandler* sensorHandler;   // used to call update() during blocking waits
    int32_t audioBuffer[512];       // 32-bit to match I2S_BITS_PER_SAMPLE_32BIT

    void configureI2S();
    void readAudioSample();
    float calculateEnergy();
    bool detectVoiceActivity(float energy);  // energy VAD — replace with ESP-SR WakeNet for production
};

#endif // VOICE_RECOGNITION_H
