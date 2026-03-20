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
 *   Name files: 0001.mp3, 0002.mp3 ... 0049.mp3 matching AudioTrack enum order.
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
    // Wake & Security
    if (strcmp(message, "ECO activated. Listening for secret code.") == 0)              return TRACK_LISTENING_CODE;
    if (strcmp(message, "Secret code correct. System unlocked.") == 0)                  return TRACK_UNLOCKED;
    if (strcmp(message, "Wrong code. Try again.") == 0)                                 return TRACK_WRONG_CODE;
    if (strcmp(message, "Time expired. System locked.") == 0)                           return TRACK_AUTH_TIMEOUT;
    if (strcmp(message, "System locked successfully.") == 0)                            return TRACK_LOCKED;
    if (strcmp(message, "System is currently locked. Please say ECO to unlock.") == 0)  return TRACK_SYSTEM_LOCKED_MSG;

    // General Listening & Confirmation
    if (strcmp(message, "Listening for command.") == 0)                                 return TRACK_LISTENING_CMD;
    if (strcmp(message, "Command received.") == 0)                                      return TRACK_CMD_RECEIVED;
    if (strcmp(message, "Sorry, I did not understand. Please repeat.") == 0)            return TRACK_CMD_UNKNOWN;
    if (strcmp(message, "Processing your request.") == 0)                               return TRACK_PROCESSING;

    // Light Control
    if (strcmp(message, "Turning on the light.") == 0)                                  return TRACK_LIGHT_TURNING_ON;
    if (strcmp(message, "Turning off the light.") == 0)                                 return TRACK_LIGHT_TURNING_OFF;
    if (strcmp(message, "Light is already on.") == 0)                                   return TRACK_LIGHT_ALREADY_ON;
    if (strcmp(message, "Light is already off.") == 0)                                  return TRACK_LIGHT_ALREADY_OFF;
    if (strcmp(message, "It is already bright. Do you still want to turn on the light?") == 0) return TRACK_BRIGHT;
    if (strcmp(message, "Light turned on successfully.") == 0)                          return TRACK_LIGHT_ON;
    if (strcmp(message, "Light remains off.") == 0)                                     return TRACK_LIGHT_REMAINS_OFF;

    // Fan Control
    if (strcmp(message, "Turning on the fan.") == 0)                                    return TRACK_FAN_TURNING_ON;
    if (strcmp(message, "Turning off the fan.") == 0)                                   return TRACK_FAN_TURNING_OFF;
    if (strcmp(message, "Fan is already running.") == 0)                                return TRACK_FAN_ALREADY_ON;
    if (strcmp(message, "Fan is already off.") == 0)                                    return TRACK_FAN_ALREADY_OFF;
    if (strcmp(message, "Temperature is low. Do you still want to turn on the fan?") == 0) return TRACK_LOW_TEMP_HUM;
    if (strcmp(message, "Fan turned on successfully.") == 0)                            return TRACK_FAN_ON;
    if (strcmp(message, "Fan remains off.") == 0)                                       return TRACK_FAN_REMAINS_OFF;

    // Motion Detection
    if (strcmp(message, "No motion detected. Do you still want to continue?") == 0)     return TRACK_NO_MOTION;
    if (strcmp(message, "Motion detected.") == 0)                                       return TRACK_MOTION_DETECTED;

    // Current & Voltage Monitoring
    if (strcmp(message, "Current is within normal range.") == 0)                        return TRACK_CURRENT_NORMAL;
    if (strcmp(message, "Warning. Low voltage detected.") == 0)                         return TRACK_LOW_VOLTAGE;
    if (strcmp(message, "Warning. Voltage fluctuation detected.") == 0)                 return TRACK_VOLT_FLUCTUATION;
    if (strcmp(message, "Overcurrent detected. Please check the appliance.") == 0)      return TRACK_OVERCURRENT;

    // Temperature & Humidity
    if (strcmp(message, "The current temperature is.") == 0)                            return TRACK_TEMP_PREFIX;
    if (strcmp(message, "Degrees Celsius.") == 0)                                       return TRACK_TEMP_SUFFIX;
    if (strcmp(message, "The current humidity is.") == 0)                               return TRACK_HUMIDITY_PREFIX;
    if (strcmp(message, "Percent.") == 0)                                               return TRACK_HUMIDITY_SUFFIX;

    // Status Report
    if (strcmp(message, "Here is the system status.") == 0)                             return TRACK_STATUS_INTRO;
    if (strcmp(message, "Light is currently on.") == 0)                                 return TRACK_STATUS_LIGHT_ON;
    if (strcmp(message, "Light is currently off.") == 0)                                return TRACK_STATUS_LIGHT_OFF;
    if (strcmp(message, "Fan is currently on.") == 0)                                   return TRACK_STATUS_FAN_ON;
    if (strcmp(message, "Fan is currently off.") == 0)                                  return TRACK_STATUS_FAN_OFF;
    if (strcmp(message, "Motion status updated.") == 0)                                 return TRACK_STATUS_MOTION_UPD;

    // Yes / No Confirmation
    if (strcmp(message, "Please say yes or no.") == 0)                                  return TRACK_ASK_YES_NO;
    if (strcmp(message, "Action confirmed.") == 0)                                      return TRACK_ACTION_CONFIRMED;
    if (strcmp(message, "Action cancelled.") == 0)                                      return TRACK_ACTION_CANCELLED;

    // System States
    if (strcmp(message, "System ready.") == 0)                                          return TRACK_SYSTEM_READY;
    if (strcmp(message, "System entering sleep mode.") == 0)                            return TRACK_SLEEP_MODE;
    if (strcmp(message, "Microphone activated.") == 0)                                  return TRACK_MIC_ACTIVATED;
    if (strcmp(message, "Goodbye.") == 0)                                               return TRACK_GOODBYE;

    // Optional
    if (strcmp(message, "Edge processing active.") == 0)                                return TRACK_EDGE_ACTIVE;
    if (strcmp(message, "All systems functioning normally.") == 0)                      return TRACK_ALL_OK;

    // Unknown — log and play "did not understand" track
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
