/**
 * GsmAsync01Minimal.ino -- GsmAsync is a non-blocking GSM module communication 
 *   manager library.
 *   https://github.com/prampec/GsmAsync
 *
 * Copyright (C) 2022 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * Example: Minimal
 * Description:
 *   After establishing connection with the GSM module, this code
 *   will regularly request the signal quality.
 * Harware setup:
 *   This code was tested with SIM800L breakout module with a RESET 
 *   and ENABLE pin. Wiring is defined below.
 */

#include <Arduino.h>
#include <GsmAsync.h>

#define PIN_GSM_ENABLE 9
#define PIN_GSM_RST 46

#define GSM_TALK_FREQ_MS 20000 // -- Talk to the GSM module in every 20 seconds.

void initGsmModule(void);
void handleCsq(char* result);
void timeoutHandler();
void errorHandler();

GsmHandler csqHandler = { "+CSQ:", handleCsq };

HardwareSerial* gsm = &Serial1;

GsmAsync gsmAsync;
unsigned long lastSendTimeMs = 0;

void setup()
{
  pinMode(PIN_GSM_ENABLE, INPUT_PULLUP);
  gsm->begin(19200);
  Serial.begin(19200);

  gsmAsync.init(gsm);
  gsmAsync.registerHandler(&csqHandler);

  initGsmModule();

  Serial.println(F("Ready."));
}

void initGsmModule(void)
{
  Serial.print(F("Reseting GSM.."));

  pinMode(PIN_GSM_RST, OUTPUT);
  digitalWrite(PIN_GSM_RST, LOW);
  delay(120);
  Serial.print(F("."));
  digitalWrite(PIN_GSM_RST, HIGH);
  delay(3000);
  gsmAsync.addCommand("ATE0", 1000); // -- Turn off echo
  Serial.println(F("done."));
}

void loop()
{
  gsmAsync.doLoop();

  unsigned long now = millis();
  if (GSM_TALK_FREQ_MS < (now - lastSendTimeMs))
  {
    Serial.println(F("Requesting signal quality report..."));
    gsmAsync.addCommand("AT+CSQ"); // Signal quality report
    lastSendTimeMs = now;
  }
}

void handleCsq(char* result)
{
  int rssi;
  sscanf(result, "%d", &rssi);
  Serial.print(F("Signal quality: "));
  Serial.println(rssi);
}

void timeoutHandler()
{
  Serial.println(F("GSM not responding"));
}

void errorHandler()
{
  Serial.println(F("GSM Error"));
}

