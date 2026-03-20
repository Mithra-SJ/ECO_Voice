/*
 * Audio Handler - Voice Feedback System
 * DFPlayer Mini based audio playback for all system responses.
 *
 * SD Card track layout (files in /mp3 folder, named 0001.mp3 ... 0049.mp3):
 *
 * --- Wake & Security ---
 *   01 - "ECO activated. Listening for secret code."
 *   02 - "Secret code correct. System unlocked."
 *   03 - "Wrong code. Try again."
 *   04 - "Time expired. System locked."
 *   05 - "System locked successfully."
 *   06 - "System is currently locked. Please say ECO to unlock."
 *
 * --- General Listening & Confirmation ---
 *   07 - "Listening for command."
 *   08 - "Command received."
 *   09 - "Sorry, I did not understand. Please repeat."
 *   10 - "Processing your request."
 *
 * --- Light Control ---
 *   11 - "Turning on the light."
 *   12 - "Turning off the light."
 *   13 - "Light is already on."
 *   14 - "Light is already off."
 *   15 - "It is already bright. Do you still want to turn on the light?"
 *   16 - "Light turned on successfully."
 *   17 - "Light remains off."
 *
 * --- Fan Control ---
 *   18 - "Turning on the fan."
 *   19 - "Turning off the fan."
 *   20 - "Fan is already running."
 *   21 - "Fan is already off."
 *   22 - "Temperature is low. Do you still want to turn on the fan?"
 *   23 - "Fan turned on successfully."
 *   24 - "Fan remains off."
 *
 * --- Motion Detection ---
 *   25 - "No motion detected. Do you still want to continue?"
 *   26 - "Motion detected."
 *
 * --- Current & Voltage Monitoring ---
 *   27 - "Current is within normal range."
 *   28 - "Warning. Low voltage detected."
 *   29 - "Warning. Voltage fluctuation detected."
 *   30 - "Overcurrent detected. Please check the appliance."
 *
 * --- Temperature & Humidity ---
 *   31 - "The current temperature is."
 *   32 - "Degrees Celsius."
 *   33 - "The current humidity is."
 *   34 - "Percent."
 *
 * --- Status Report ---
 *   35 - "Here is the system status."
 *   36 - "Light is currently on."
 *   37 - "Light is currently off."
 *   38 - "Fan is currently on."
 *   39 - "Fan is currently off."
 *   40 - "Motion status updated."
 *
 * --- Yes / No Confirmation ---
 *   41 - "Please say yes or no."
 *   42 - "Action confirmed."
 *   43 - "Action cancelled."
 *
 * --- System States ---
 *   44 - "System ready."
 *   45 - "System entering sleep mode."
 *   46 - "Microphone activated."
 *   47 - "Goodbye."
 *
 * --- Optional ---
 *   48 - "Edge processing active."
 *   49 - "All systems functioning normally."
 */

#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <Arduino.h>
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
