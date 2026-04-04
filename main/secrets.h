/*
 * secrets.h — Credentials file. This file is NOT tracked in git.
 *
 * SETUP:
 *   Edit this file and change SECRET_CODE to your own phrase.
 *   Keep this file secure and never commit it.
 */

#ifndef SECRETS_H
#define SECRETS_H

#define SECRET_CODE        "1450"
// Spoken form of SECRET_CODE registered with ESP-SR MultiNet.
// Must be a pronounceable English phrase the model can recognize.
// Change both values together if you update the secret code.
#define SECRET_CODE_PHRASE "one four five zero"

#endif // SECRETS_H