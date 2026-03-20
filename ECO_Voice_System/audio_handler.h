/*
 * Audio Handler - Voice Feedback System
 * DFPlayer Mini based audio playback for all system responses.
 *
 * SD Card track layout (files must be named 0001.mp3, 0002.mp3 ... in /mp3 folder):
 *   01 - "Listening for secret code"
 *   02 - "Unlocked. Ready for commands"
 *   03 - "Wrong code. Try again"
 *   04 - "Authentication timeout"
 *   05 - "System locked"
 *   06 - "Light turned on"
 *   07 - "Light turned off"
 *   08 - "Fan turned on"
 *   09 - "Fan turned off"
 *   10 - "No motion detected. Do you still want to continue?"
 *   11 - "It's bright already. Do you still want to switch on light?"
 *   12 - "High current detected. Do you still want to turn on fan?"
 *   13 - "Light not turned on"
 *   14 - "Fan not turned on"
 *   15 - "Fan not turned on for safety"
 *   16 - "Command not recognized"
 *   17 - "Motion detected"
 *   18 - "No motion"
 *   19 - "Light is on"
 *   20 - "Light is off"
 *   21 - "Fan is on"
 *   22 - "Fan is off"
 */

#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

// Audio track numbers matching SD card files
enum AudioTrack {
    TRACK_LISTENING_CODE      = 1,
    TRACK_UNLOCKED            = 2,
    TRACK_WRONG_CODE          = 3,
    TRACK_AUTH_TIMEOUT        = 4,
    TRACK_LOCKED              = 5,
    TRACK_LIGHT_ON            = 6,
    TRACK_LIGHT_OFF           = 7,
    TRACK_FAN_ON              = 8,
    TRACK_FAN_OFF             = 9,
    TRACK_NO_MOTION           = 10,
    TRACK_BRIGHT              = 11,
    TRACK_HIGH_CURRENT        = 12,
    TRACK_LIGHT_NOT_ON        = 13,
    TRACK_FAN_NOT_ON          = 14,
    TRACK_FAN_NOT_ON_SAFETY   = 15,
    TRACK_CMD_UNKNOWN         = 16,
    TRACK_STATUS_MOTION_YES   = 17,
    TRACK_STATUS_MOTION_NO    = 18,
    TRACK_STATUS_LIGHT_ON     = 19,
    TRACK_STATUS_LIGHT_OFF    = 20,
    TRACK_STATUS_FAN_ON       = 21,
    TRACK_STATUS_FAN_OFF      = 22,
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
