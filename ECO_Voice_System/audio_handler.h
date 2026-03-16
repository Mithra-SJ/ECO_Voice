/*
 * Audio Handler - Voice Feedback System
 * Handles audio playback and text-to-speech functionality
 */

#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <Arduino.h>

class AudioHandler {
public:
    AudioHandler();
    bool init();
    void speak(const char* message);
    void playTone(int frequency, int duration);
    void playConfirmation();
    void playError();

private:
    bool initialized;
    void speakWord(const char* word);
};

#endif // AUDIO_HANDLER_H
