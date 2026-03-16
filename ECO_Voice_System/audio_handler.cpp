/*
 * Audio Handler Implementation
 */

#include "audio_handler.h"
#include "config.h"

AudioHandler::AudioHandler() : initialized(false) {
}

bool AudioHandler::init() {
    // Initialize speaker/buzzer if using hardware TTS
    // For this implementation, we'll use Serial output as placeholder
    // In production, integrate ESP8266Audio library or DFPlayer Mini

    Serial.println("Audio Handler Initialized");
    Serial.println("Note: Voice feedback shown on Serial Monitor");
    Serial.println("For actual audio, integrate DFPlayer Mini or speaker module");

    initialized = true;
    return true;
}

void AudioHandler::speak(const char* message) {
    if (!initialized) return;

    // Print to serial for debugging
    Serial.print("[AUDIO] ");
    Serial.println(message);

    // TODO: Implement actual TTS or play pre-recorded audio files
    // Options:
    // 1. Use DFPlayer Mini module with pre-recorded MP3 files
    // 2. Use WT2003S-24SS voice module
    // 3. Use ESP32 DAC with MBROLA/eSpeak synthesis
    // 4. Use cloud TTS (not offline)

    // For now, play confirmation beep
    playTone(1000, 100);

    delay(500); // Simulate speech duration
}

void AudioHandler::speakWord(const char* word) {
    // Helper function for individual words
    speak(word);
}

void AudioHandler::playTone(int frequency, int duration) {
    // Simple tone generation using ESP32 LEDC
    // Could also use a buzzer on a GPIO pin

    // For actual implementation, use:
    // ledcSetup(0, frequency, 8);
    // ledcAttachPin(BUZZER_PIN, 0);
    // ledcWrite(0, 128);
    // delay(duration);
    // ledcWrite(0, 0);

    Serial.print("[TONE] ");
    Serial.print(frequency);
    Serial.print("Hz for ");
    Serial.print(duration);
    Serial.println("ms");
}

void AudioHandler::playConfirmation() {
    playTone(1500, 100);
    delay(50);
    playTone(2000, 100);
}

void AudioHandler::playError() {
    playTone(500, 200);
    delay(100);
    playTone(400, 200);
}

/* ===== IMPLEMENTATION GUIDE FOR AUDIO OUTPUT =====
 *
 * METHOD 1: DFPlayer Mini (Recommended for simplicity)
 * ----------------------------------------------------
 * - Use pre-recorded MP3 files
 * - Store messages like "unlocked.mp3", "light_on.mp3", etc.
 * - Connect: TX→RX, RX→TX, VCC→5V, GND→GND
 * - Library: DFRobotDFPlayerMini
 * - Pros: Clear audio, easy to update messages
 * - Cons: Requires SD card, pre-recording
 *
 * CODE EXAMPLE:
 * #include <DFRobotDFPlayerMini.h>
 * DFRobotDFPlayerMini myDFPlayer;
 * myDFPlayer.play(1); // Play first track
 *
 * METHOD 2: MAX98357A I2S Amplifier + Speaker
 * --------------------------------------------
 * - Generate audio using ESP32-audioI2S library
 * - Use TTS library or pre-generated WAV files
 * - Better quality, more flexible
 * - Requires more storage and processing
 *
 * METHOD 3: Simple Buzzer with Morse Code/Patterns
 * -------------------------------------------------
 * - Use different beep patterns for different messages
 * - Very simple hardware (just a buzzer)
 * - Limited feedback capability
 *
 * Choose based on your needs and available components.
 */
