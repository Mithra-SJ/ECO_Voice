/*
 * appliance_control.h — ECO Voice Online Version
 * Controls relays and status LEDs via Arduino GPIO
 */

#ifndef APPLIANCE_CONTROL_H
#define APPLIANCE_CONTROL_H

#include <Arduino.h>
#include "config.h"

class ApplianceControl {
public:
    ApplianceControl();
    void init();

    void setLight(bool on);
    void setFan(bool on);
    void turnOffAll();

    bool isLightOn() const { return lightOn; }
    bool isFanOn() const   { return fanOn; }

    // Green LED = online, Red LED = offline/error
    void setOnlineLED(bool online);

private:
    bool lightOn;
    bool fanOn;

    void writeRelay(int pin, bool on);
};

#endif // APPLIANCE_CONTROL_H
