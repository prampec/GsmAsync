/**
 * GsmAsync02NetworkStatus.ino -- GsmAsync is a non-blocking GSM module communication 
 *   manager library.
 *   https://github.com/prampec/GsmAsync
 *
 * Copyright (C) 2022 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

/**
 * Example: NetworkStatus
 * Description:
 *   This example regularly check network connection status and
 *   reports it to the serial console.
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
void handleReg(char* result);
void handleSpn(char* result);
void handleCsq(char* result);
void timeoutHandler();
void errorHandler();

GsmHandler regHandler = { "+CREG:", handleReg }; // Network registration
GsmHandler spnHandler = { "+CSPN:", handleSpn }; // Service provider name
GsmHandler csqHandler = { "+CSQ:", handleCsq }; // Signam quality

HardwareSerial* gsm = &Serial1;

GsmAsync gsmAsync;
unsigned long lastSendTimeMs = 0;
int networkState = 4;

void setup()
{
  pinMode(PIN_GSM_ENABLE, INPUT_PULLUP);
  gsm->begin(19200);
  Serial.begin(19200);

  gsmAsync.init(gsm, timeoutHandler, errorHandler);
  gsmAsync.registerHandler(&regHandler);
  gsmAsync.registerHandler(&spnHandler);
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
  if ((networkState != 1)
    && (networkState != 3)
    && (networkState != 5)
    && (500 < (now - lastSendTimeMs)))
  {
    Serial.print(F("."));
    gsmAsync.addCommand("AT+CREG?"); // Network registration check
    lastSendTimeMs = now;
  }
  else if (GSM_TALK_FREQ_MS < (now - lastSendTimeMs))
  {
    Serial.println(F("Requesting module status..."));
    gsmAsync.addCommand("AT+CSQ"); // Signal quality report
    gsmAsync.addCommand("AT+CREG?"); // Network registration check
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

void handleReg(char* result)
{
  int n;
  int stat;
  sscanf(result, "%d,%d", &n, &stat);
  if (networkState != stat)
  {
    if (stat == 2)
    {
      Serial.println(F("Connecting to network"));
    }
    else if (stat == 1)
    {
      Serial.println(F("Connected to network"));
      gsmAsync.addCommand("AT+CSPN?"); // -- Ask for service provider name
      gsmAsync.addCommand("AT+CSQ"); // Signal quality report
    }
    else if (stat == 3)
    {
      Serial.println(F("Connection denied"));
    }
    else if (stat == 5)
    {
      Serial.println(F("Roaming"));
      gsmAsync.addCommand("AT+CSPN?");
    }
    else if (stat == 4)
    {
      // Do nothing
    }
    else
    {
      Serial.println(F("Unexpeced connection problem."));
    }
    networkState = stat;
  }
}

void handleSpn(char* result)
{
  // -- Serice provider name arrived
  char spn[GSMASYNC_BUF_SIZE];
  char *p = strchr(result, ',');
  if (p != NULL)
  {
    int index = (int)(p - result);
    strncpy(spn, result, index);
    spn[index] = '\0';
  }
  Serial.print(F("Service provider is: "));
  Serial.println(spn);
}

void timeoutHandler()
{
  Serial.println(F("GSM not responding"));
}

void errorHandler()
{
  Serial.println(F("GSM Error"));
}

