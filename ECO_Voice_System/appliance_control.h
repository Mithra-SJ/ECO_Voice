/*
 * Appliance Control - Manages relays and status LEDs
 */

#ifndef APPLIANCE_CONTROL_H
#define APPLIANCE_CONTROL_H

#include <Arduino.h>

class ApplianceControl {
public:
    ApplianceControl();
    void init();

    // Relay Control
    void setLight(bool state);
    void setFan(bool state);
    void turnOffAll();

    // Status Query
    bool isLightOn();
    bool isFanOn();

    // LED Status Indicators
    void setStatusLED(bool unlocked);  // true = green (unlocked), false = red (locked)
    void blinkStatusLED(bool green, int times);

private:
    bool lightState;
    bool fanState;

    void updateRelay(int pin, bool state);
};

#endif // APPLIANCE_CONTROL_H
