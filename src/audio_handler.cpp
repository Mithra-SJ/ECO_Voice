/*
 * Audio Handler Implementation — Serial-only (ESP-IDF)
 *
 * Simplified to use Serial logging only since DFPlayer requires Arduino framework.
 * User can implement TTS or audio feedback separately if needed.
 */

#include "audio_handler.h"
#include "config.h"
#include "esp_log.h"

AudioHandler::AudioHandler() : initialized(false) {
}

bool AudioHandler::init() {
    ESP_LOGI("AUDIO", "Initializing Audio Handler (Serial-only)...");
    ESP_LOGW("AUDIO", "DFPlayer not available in ESP-IDF. Using Serial feedback only.");
    initialized = true;
    return true;
}

void AudioHandler::speak(const char* message) {
    ESP_LOGI("AUDIO", "Speaking: %s", message);
    // TODO: Implement TTS or audio playback here if needed
}

void AudioHandler::speak(AudioTrack track) {
    ESP_LOGI("AUDIO", "Playing track %d", (int)track);
    // TODO: Implement audio playback here if needed
}

AudioTrack AudioHandler::messageToTrack(const char* message) {
    // Simplified - just return unknown for now
    ESP_LOGW("AUDIO", "Track mapping not implemented: %s", message);
    return TRACK_CMD_UNKNOWN;
}

void AudioHandler::playConfirmation() {
    ESP_LOGI("AUDIO", "Playing confirmation sound");
}

void AudioHandler::playError() {
    ESP_LOGI("AUDIO", "Playing error sound");
}
