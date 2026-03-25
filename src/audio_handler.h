/*
 * Audio Handler — DFPlayer Mini via ESP-IDF UART direct protocol
 *
 * No Arduino library dependency. DFPlayer Mini is driven directly
 * using ESP-IDF UART1 with the 10-byte serial frame protocol.
 *
 * Frame: 0x7E 0xFF 0x06 CMD 0x00 P1 P2 CS_H CS_L 0xEF
 * Checksum = 2's complement of (0xFF + 0x06 + CMD + 0x00 + P1 + P2)
 */

#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TRACK_NONE              = 0,  // sentinel — no matching track

    // Wake & Security
    TRACK_LISTENING_CODE    = 1,
    TRACK_UNLOCKED          = 2,
    TRACK_WRONG_CODE        = 3,
    TRACK_AUTH_TIMEOUT      = 4,
    TRACK_LOCKED            = 5,
    TRACK_SYSTEM_LOCKED_MSG = 6,

    // General Listening & Confirmation
    TRACK_LISTENING_CMD     = 7,
    TRACK_CMD_RECEIVED      = 8,
    TRACK_CMD_UNKNOWN       = 9,
    TRACK_PROCESSING        = 10,

    // Light Control
    TRACK_LIGHT_TURNING_ON  = 11,
    TRACK_LIGHT_TURNING_OFF = 12,
    TRACK_LIGHT_ALREADY_ON  = 13,
    TRACK_LIGHT_ALREADY_OFF = 14,
    TRACK_BRIGHT            = 15,
    TRACK_LIGHT_ON          = 16,
    TRACK_LIGHT_REMAINS_OFF = 17,

    // Fan Control
    TRACK_FAN_TURNING_ON    = 18,
    TRACK_FAN_TURNING_OFF   = 19,
    TRACK_FAN_ALREADY_ON    = 20,
    TRACK_FAN_ALREADY_OFF   = 21,
    TRACK_LOW_TEMP_HUM      = 22,
    TRACK_FAN_ON            = 23,
    TRACK_FAN_REMAINS_OFF   = 24,

    // Motion Detection
    TRACK_NO_MOTION         = 25,
    TRACK_MOTION_DETECTED   = 26,

    // Current & Voltage Monitoring
    TRACK_CURRENT_NORMAL    = 27,
    TRACK_LOW_VOLTAGE       = 28,
    TRACK_VOLT_FLUCTUATION  = 29,
    TRACK_OVERCURRENT       = 30,

    // Temperature & Humidity
    TRACK_TEMP_PREFIX       = 31,
    TRACK_TEMP_SUFFIX       = 32,
    TRACK_HUMIDITY_PREFIX   = 33,
    TRACK_HUMIDITY_SUFFIX   = 34,

    // Status Report
    TRACK_STATUS_INTRO      = 35,
    TRACK_STATUS_LIGHT_ON   = 36,
    TRACK_STATUS_LIGHT_OFF  = 37,
    TRACK_STATUS_FAN_ON     = 38,
    TRACK_STATUS_FAN_OFF    = 39,
    TRACK_STATUS_MOTION_UPD = 40,

    // Yes / No Confirmation
    TRACK_ASK_YES_NO        = 41,
    TRACK_ACTION_CONFIRMED  = 42,
    TRACK_ACTION_CANCELLED  = 43,

    // System States
    TRACK_SYSTEM_READY      = 44,
    TRACK_SLEEP_MODE        = 45,
    TRACK_MIC_ACTIVATED     = 46,
    TRACK_GOODBYE           = 47,
    TRACK_EDGE_ACTIVE       = 48,
    TRACK_ALL_OK            = 49,

    // Auth lockout — add 0050.mp3 to SD card:
    //   "Too many wrong attempts. Please wait 30 seconds."
    TRACK_AUTH_LOCKOUT      = 50,
} AudioTrack;

class AudioHandler {
public:
    AudioHandler();
    bool init();
    void speak(const char* message);   // maps message string → DFPlayer track
    void speak(AudioTrack track);      // plays track number directly
    void playConfirmation();
    void playError();

private:
    bool initialized;
    void sendFrame(uint8_t cmd, uint8_t param1, uint8_t param2);
    AudioTrack messageToTrack(const char* message);
};

#endif // AUDIO_HANDLER_H
