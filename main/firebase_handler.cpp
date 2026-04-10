/*
 * firebase_handler.cpp — ECO Voice Online Version
 */

#include "firebase_handler.h"
#include "secrets.h"

static void tokenStatusCallback(TokenInfo info) {
    if (info.status == token_status_error) {
        Serial.printf("[FIREBASE] Token error: %s\n", info.error.message.c_str());
    }
}

FirebaseHandler::FirebaseHandler() : ready(false) {}

void FirebaseHandler::init() {
    config.api_key      = FIREBASE_API_KEY;
    config.database_url = FIREBASE_DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;

    auth.user.email    = FIREBASE_DEVICE_EMAIL;
    auth.user.password = FIREBASE_DEVICE_PASSWORD;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Serial.print("[FIREBASE] Authenticating");
    unsigned long start = millis();
    while (!Firebase.ready()) {
        if (millis() - start > 15000) {
            Serial.println("\n[FIREBASE] Auth timeout — check credentials in secrets.h");
            return;
        }
        delay(300);
        Serial.print(".");
    }
    Serial.println("\n[FIREBASE] Connected and ready.");
    ready = true;
}

bool FirebaseHandler::isReady() {
    return ready && Firebase.ready();
}

void FirebaseHandler::pushSensors(SensorHandler& sensors) {
    if (!isReady()) return;

    FirebaseJson json;
    json.set("temperature", sensors.getTemperature());
    json.set("humidity",    sensors.getHumidity());
    json.set("motion",      sensors.isMotionDetected());
    json.set("light_level", sensors.getLightLevel());
    json.set("current",     sensors.getCurrent());
    json.set("voltage",     sensors.getVoltage());
    json.set("power",       sensors.getPower());

    if (!Firebase.RTDB.updateNode(&fbdo, "/eco_voice/sensors", &json)) {
        Serial.printf("[FIREBASE] Sensor push failed: %s\n", fbdo.errorReason().c_str());
    }
}

void FirebaseHandler::pushStatus(ApplianceControl& appliances) {
    if (!isReady()) return;

    FirebaseJson json;
    json.set("light_on",  appliances.isLightOn());
    json.set("fan_on",    appliances.isFanOn());
    json.set("last_seen/.sv", "timestamp");  // Firebase server timestamp

    if (!Firebase.RTDB.updateNode(&fbdo, "/eco_voice/status", &json)) {
        Serial.printf("[FIREBASE] Status push failed: %s\n", fbdo.errorReason().c_str());
    }
}

void FirebaseHandler::writeCommand(const char* device, bool state) {
    if (!isReady()) return;
    String path = String("/eco_voice/commands/") + device;
    if (!Firebase.RTDB.setBool(&fbdo, path.c_str(), state)) {
        Serial.printf("[FIREBASE] writeCommand failed: %s\n", fbdo.errorReason().c_str());
    }
}

void FirebaseHandler::checkCommands(ApplianceControl& appliances) {
    if (!isReady()) return;

    // Check light command
    if (Firebase.RTDB.getBool(&fbdoCmd, "/eco_voice/commands/light")) {
        bool cmd = fbdoCmd.boolData();
        if (cmd != appliances.isLightOn()) {
            appliances.setLight(cmd);
            // Confirm state back to Firebase immediately
            Firebase.RTDB.setBool(&fbdo, "/eco_voice/status/light_on", cmd);
        }
    }

    // Check fan command
    if (Firebase.RTDB.getBool(&fbdoCmd, "/eco_voice/commands/fan")) {
        bool cmd = fbdoCmd.boolData();
        if (cmd != appliances.isFanOn()) {
            appliances.setFan(cmd);
            Firebase.RTDB.setBool(&fbdo, "/eco_voice/status/fan_on", cmd);
        }
    }
}
