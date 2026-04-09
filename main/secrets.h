/*
 * secrets.h — Credentials file. Not committed to git.
 */

#ifndef SECRETS_H
#define SECRETS_H

// WiFi
#define WIFI_SSID        "LICET_STAFF"
#define WIFI_PASSWORD    "Licet@3111"

// Firebase
#define FIREBASE_API_KEY        "AIzaSyAoVKRRs_oyRWM9f56N7AoVDlAX2o0DF5w"
#define FIREBASE_DATABASE_URL   "https://eco-voice-ad60e-default-rtdb.firebaseio.com"

// Firebase device account (ESP32 logs in as this user)
#define FIREBASE_DEVICE_EMAIL    "device@ecovoice.local"
#define FIREBASE_DEVICE_PASSWORD "device.local@123"

#endif // SECRETS_H
