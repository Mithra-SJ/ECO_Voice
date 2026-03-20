/*
 * Audio Handler Implementation — DFPlayer Mini
 *
 * Hardware: DFPlayer Mini connected via Serial1
 *   ESP32 GPIO 16 (TX) → DFPlayer RX (via 1kΩ resistor)
 *   ESP32 GPIO 15 (RX) ← DFPlayer TX
 *   DFPlayer VCC → 5V, GND → GND
 *   Speaker (4Ω/8Ω, 3W) connected to DFPlayer SPK_1 and SPK_2
 *
 * SD Card setup:
 *   Create folder /mp3 on SD card root.
 *   Name files: 0001.mp3, 0002.mp3 ... matching AudioTrack enum order.
 */

#include "audio_handler.h"
#include "config.h"

AudioHandler::AudioHandler() : initialized(false), dfSerial(&Serial1) {
}

bool AudioHandler::init() {
    pinMode(DFPLAYER_BUSY_PIN, INPUT);  // LOW=playing, HIGH=idle
    dfSerial->begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);

    Serial.println("Initializing DFPlayer Mini...");

    if (!dfPlayer.begin(*dfSerial)) {
        Serial.println("[AUDIO] DFPlayer init failed. Check wiring and SD card.");
        Serial.println("[AUDIO] Falling back to Serial-only feedback.");
        initialized = false;
        return false;
    }

    dfPlayer.volume(25);  // Volume: 0-30
    Serial.println("[AUDIO] DFPlayer ready.");
    initialized = true;
    return true;
}

void AudioHandler::speak(const char* message) {
    Serial.print("[AUDIO] ");
    Serial.println(message);

    AudioTrack track = messageToTrack(message);
    speak(track);
}

void AudioHandler::speak(AudioTrack track) {
    if (!initialized) return;
    dfPlayer.play((int)track);

    // Two-phase BUSY pin wait:
    // Phase 1 — wait for BUSY to go LOW (DFPlayer started playing).
    //            500ms guard prevents infinite hang if BUSY pin is unwired.
    // Phase 2 — wait for BUSY to go HIGH (playback finished).
    delay(50);
    unsigned long startWait = millis();
    while (digitalRead(DFPLAYER_BUSY_PIN) == HIGH && millis() - startWait < 500) {
        delay(10);
    }
    while (digitalRead(DFPLAYER_BUSY_PIN) == LOW) {
        delay(50);
    }
    delay(100); // brief pause between clips
}

AudioTrack AudioHandler::messageToTrack(const char* message) {
    // strcmp() — zero heap allocation, direct char* comparison
    if (strcmp(message, "Listening for secret code") == 0)                                  return TRACK_LISTENING_CODE;
    if (strcmp(message, "Unlocked. Ready for commands") == 0)                               return TRACK_UNLOCKED;
    if (strcmp(message, "Wrong code. Try again") == 0)                                      return TRACK_WRONG_CODE;
    if (strcmp(message, "Authentication timeout") == 0)                                     return TRACK_AUTH_TIMEOUT;
    if (strcmp(message, "System locked") == 0)                                              return TRACK_LOCKED;
    if (strcmp(message, "Light turned on") == 0)                                            return TRACK_LIGHT_ON;
    if (strcmp(message, "Light turned off") == 0)                                           return TRACK_LIGHT_OFF;
    if (strcmp(message, "Fan turned on") == 0)                                              return TRACK_FAN_ON;
    if (strcmp(message, "Fan turned off") == 0)                                             return TRACK_FAN_OFF;
    if (strcmp(message, "No motion detected. Do you still want to continue?") == 0)         return TRACK_NO_MOTION;
    if (strcmp(message, "It's bright already. Do you still want to switch on light?") == 0) return TRACK_BRIGHT;
    if (strcmp(message, "High current detected. Do you still want to turn on fan?") == 0)   return TRACK_HIGH_CURRENT;
    if (strcmp(message, "Light not turned on") == 0)                                        return TRACK_LIGHT_NOT_ON;
    if (strcmp(message, "Fan not turned on") == 0)                                          return TRACK_FAN_NOT_ON;
    if (strcmp(message, "Fan not turned on for safety") == 0)                               return TRACK_FAN_NOT_ON_SAFETY;
    if (strcmp(message, "Command not recognized") == 0)                                     return TRACK_CMD_UNKNOWN;
    if (strcmp(message, "Motion detected.") == 0)                                           return TRACK_STATUS_MOTION_YES;
    if (strcmp(message, "No motion.") == 0)                                                 return TRACK_STATUS_MOTION_NO;
    if (strcmp(message, "Light is on.") == 0)                                               return TRACK_STATUS_LIGHT_ON;
    if (strcmp(message, "Light is off.") == 0)                                              return TRACK_STATUS_LIGHT_OFF;
    if (strcmp(message, "Fan is on.") == 0)                                                 return TRACK_STATUS_FAN_ON;
    if (strcmp(message, "Fan is off.") == 0)                                                return TRACK_STATUS_FAN_OFF;

    // Unknown message — log and return CMD_UNKNOWN track.
    // Caller (speak(const char*)) will play it once. Do NOT call playError() here.
    Serial.print("[AUDIO] No track mapped for: ");
    Serial.println(message);
    return TRACK_CMD_UNKNOWN;
}

void AudioHandler::playConfirmation() {
    speak(TRACK_UNLOCKED);
}

void AudioHandler::playError() {
    speak(TRACK_WRONG_CODE);
}
