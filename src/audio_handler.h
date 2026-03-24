/*
 * Audio Handler - Voice Feedback System (ESP-IDF)
 * Simplified serial-only implementation.
 */

#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// Audio track enumeration (simplified)
typedef enum {
    TRACK_LISTENING_CODE = 1,
    TRACK_UNLOCKED,
    TRACK_WRONG_CODE,
    TRACK_AUTH_TIMEOUT,
    TRACK_LOCKED,
    TRACK_SYSTEM_LOCKED_MSG,
    TRACK_LISTENING_CMD,
    TRACK_CMD_RECEIVED,
    TRACK_CMD_UNKNOWN,
    TRACK_PROCESSING,
    TRACK_LIGHT_ON,
    TRACK_LIGHT_OFF,
    TRACK_FAN_ON,
    TRACK_FAN_OFF,
    TRACK_STATUS,
    TRACK_NO_MOTION,
    TRACK_BRIGHT,
    TRACK_LOW_TEMP_HUM,
    TRACK_LOW_VOLTAGE,
    TRACK_VOLT_FLUCTUATION,
    TRACK_SYSTEM_READY
} AudioTrack;

class AudioHandler {
public:
    AudioHandler();
    bool init();
    void speak(const char* message);
    void speak(AudioTrack track);
    AudioTrack messageToTrack(const char* message);
    void playConfirmation();
    void playError();

private:
    bool initialized;
};

#endif // AUDIO_HANDLER_H

#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <stdint.h>
#include <DFRobotDFPlayerMini.h>

enum AudioTrack {
    // Wake & Security
    TRACK_LISTENING_CODE      = 1,
    TRACK_UNLOCKED            = 2,
    TRACK_WRONG_CODE          = 3,
    TRACK_AUTH_TIMEOUT        = 4,
    TRACK_LOCKED              = 5,
    TRACK_SYSTEM_LOCKED_MSG   = 6,

    // General Listening & Confirmation
    TRACK_LISTENING_CMD       = 7,
    TRACK_CMD_RECEIVED        = 8,
    TRACK_CMD_UNKNOWN         = 9,
    TRACK_PROCESSING          = 10,

    // Light Control
    TRACK_LIGHT_TURNING_ON    = 11,
    TRACK_LIGHT_TURNING_OFF   = 12,
    TRACK_LIGHT_ALREADY_ON    = 13,
    TRACK_LIGHT_ALREADY_OFF   = 14,
    TRACK_BRIGHT              = 15,
    TRACK_LIGHT_ON            = 16,
    TRACK_LIGHT_REMAINS_OFF   = 17,

    // Fan Control
    TRACK_FAN_TURNING_ON      = 18,
    TRACK_FAN_TURNING_OFF     = 19,
    TRACK_FAN_ALREADY_ON      = 20,
    TRACK_FAN_ALREADY_OFF     = 21,
    TRACK_LOW_TEMP_HUM        = 22,
    TRACK_FAN_ON              = 23,
    TRACK_FAN_REMAINS_OFF     = 24,

    // Motion Detection
    TRACK_NO_MOTION           = 25,
    TRACK_MOTION_DETECTED     = 26,

    // Current & Voltage Monitoring
    TRACK_CURRENT_NORMAL      = 27,
    TRACK_LOW_VOLTAGE         = 28,
    TRACK_VOLT_FLUCTUATION    = 29,
    TRACK_OVERCURRENT         = 30,

    // Temperature & Humidity
    TRACK_TEMP_PREFIX         = 31,
    TRACK_TEMP_SUFFIX         = 32,
    TRACK_HUMIDITY_PREFIX     = 33,
    TRACK_HUMIDITY_SUFFIX     = 34,

    // Status Report
    TRACK_STATUS_INTRO        = 35,
    TRACK_STATUS_LIGHT_ON     = 36,
    TRACK_STATUS_LIGHT_OFF    = 37,
    TRACK_STATUS_FAN_ON       = 38,
    TRACK_STATUS_FAN_OFF      = 39,
    TRACK_STATUS_MOTION_UPD   = 40,

    // Yes / No Confirmation
    TRACK_ASK_YES_NO          = 41,
    TRACK_ACTION_CONFIRMED    = 42,
    TRACK_ACTION_CANCELLED    = 43,

    // System States
    TRACK_SYSTEM_READY        = 44,
    TRACK_SLEEP_MODE          = 45,
    TRACK_MIC_ACTIVATED       = 46,
    TRACK_GOODBYE             = 47,

    // Optional
    TRACK_EDGE_ACTIVE         = 48,
    TRACK_ALL_OK              = 49,
};

class AudioHandler {
public:
    AudioHandler();
    bool init();
    void speak(const char* message);   // Maps message string → DFPlayer track
    void speak(AudioTrack track);      // Direct track playback
    void playConfirmation();
    void playError();

private:
    bool initialized;
    DFRobotDFPlayerMini dfPlayer;
    HardwareSerial* dfSerial;

    AudioTrack messageToTrack(const char* message);
};

#endif // AUDIO_HANDLER_H
