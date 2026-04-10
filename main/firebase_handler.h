/*
 * firebase_handler.h — ECO Voice Online Version
 * Handles Firebase Realtime Database sync (sensor push + command poll)
 */

#ifndef FIREBASE_HANDLER_H
#define FIREBASE_HANDLER_H

#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include "sensor_handler.h"
#include "appliance_control.h"

class FirebaseHandler {
public:
    FirebaseHandler();
    void init();
    bool isReady();

    void pushSensors(SensorHandler& sensors);
    void pushStatus(ApplianceControl& appliances);
    void checkCommands(ApplianceControl& appliances);
    void writeCommand(const char* device, bool state);  // called by voice handler

private:
    FirebaseData fbdo;
    FirebaseData fbdoCmd;
    FirebaseAuth auth;
    FirebaseConfig config;
    bool ready;
};

#endif // FIREBASE_HANDLER_H
