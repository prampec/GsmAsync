/**
 * GsmAsync.h -- GsmAsync is a module helps communicate with 
 * SIM900, SIM800 modules in a non-blocking way.
 *   https://github.com/prampec/GsmAsync
 *
 * Copyright (C) 2019 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "GsmAsync.h"

static const char* GSMASYNC_OK_STR = "OK";
static const char* GSMASYNC_ERROR_STR = "ERROR";

GsmAsync::GsmAsync()
{

}
void GsmAsync::init(Stream* gsm, void (*timeoutHandler)(), void (*errorHandler)())
{
  this->_gsm = gsm;
  this->_timeoutHandler = timeoutHandler;
  this->_errorHandler = errorHandler;
}

void GsmAsync::doLoop()
{
  while (this->_gsm->available())
  {
    if (this->_gsmState == GSMASYNC_READ_STATE_BUFF)
    {
      // -- Read result to buffer on handler-match
      boolean bufferReady = this->fillResultBuffer();
      if (bufferReady)
      {
#if GSMASYNC_DEBUG >= 1
        Serial.println();
        Serial.print((double)millis() / 1000);
        Serial.print(" Calling handler for '");
        Serial.print(this->_handlerToCall->prefix);
        Serial.print("' with argument '");
        Serial.print(this->_buffer);
        Serial.println("'.");
#endif
        (this->_handlerToCall->callback)(this->_buffer); // -- Call callback.
        this->_gsmState = GSMASYNC_READ_STATE_NORMAL;
      }
    }
    else // -- GSMASYNC_READ_STATE_NORMAL
    {
      // -- Read all characters and search for a pattern match.
      char c = this->_gsm->read();
#if GSMASYNC_DEBUG >= 3
      Serial.write(c);
#endif
      if (c < 32)
      {
        this->_okPos = 0;
        this->_errorPos = 0;
        continue;
      }
      if (checkOk(c))
      {
        this->clearSerial();
        this->handleOk();
        continue;
      }
      if (checkError(c))
      {
        clearSerial();
        this->handleError();
        continue;
      }
      this->_handlerToCall = checkGsmHandler(c);
      if (this->_handlerToCall != NULL)
      {
        this->_buffPos = 0;
        this->_gsmState = GSMASYNC_READ_STATE_BUFF;
      }
    }
  }

  this->checkTimeout();
}

void GsmAsync::clearSerial()
{
  while(this->_gsm->available())
  {
    this->_gsm->read();
  }
}

// -- TODO: How could we add OK and ERROR handlers as GsmHandler?
/**
 * Retruns true, when OK symbole received.
 */
boolean GsmAsync::checkOk(char c)
{
  if (this->_okPos < 0)
  {
    return false; // -- Start check after new line
  }
  if (GSMASYNC_OK_STR[this->_okPos] == c)
  {
    this->_okPos += 1;
    if (this->_okPos == (int)strlen(GSMASYNC_OK_STR))
    {
      this->_okPos = -1;
      return true;
    }
    return false;
  }

  this->_okPos = -1;
  return false;
}
/**
 * Retruns true, when OK symbole received.
 */
boolean GsmAsync::checkError(char c)
{
  if (this->_errorPos < 0)
  {
    return false; // -- Start check after new line
  }
  if (GSMASYNC_ERROR_STR[this->_errorPos] == c)
  {
    this->_errorPos += 1;
    if (this->_errorPos == (int)strlen(GSMASYNC_ERROR_STR))
    {
      this->_errorPos = -1;
      return true;
    }
    return false;
  }

  this->_errorPos = -1;
  return false;
}

/**
 * Retruns true, when buffer is ready.
 */
boolean GsmAsync::fillResultBuffer()
{
  while (this->_gsm->available())
  {
    char c = this->_gsm->read();
    if ((this->_buffPos == 0) && ((c == ' ') || (c == ':')))
    {
      continue; // Swallow empty characters between prefix and result.
    }
    if ((c < 32) || (this->_buffPos >= (sizeof(this->_buffer)-1)))
    {
      this->_buffer[this->_buffPos] = '\0';
      return true;
    }
    this->_buffer[this->_buffPos++] = c;
  }
  return false;
}

void GsmAsync::registerHandler(GsmHandler* handler)
{
  if (this->_firstHandler == NULL)
  {
    this->_firstHandler = handler;
    return;
  }
  GsmHandler* lastHandler = this->_firstHandler;
  while(lastHandler->nextHandler != NULL)
  {
    lastHandler = lastHandler->nextHandler;
  }
  lastHandler->nextHandler = handler;
}

/**
 * Returns the handler, when match is found.
 */
GsmHandler* GsmAsync::checkGsmHandler(char c)
{
  GsmHandler* handler = this->_firstHandler;
  while(handler != NULL)
  {
    if (handler->prefix[handler->matchPos] != c)
    {
      handler->matchPos = 0;
    }
    if (handler->prefix[handler->matchPos] == c)
    {
      // -- Has a match!
      handler->matchPos += 1;
      if (handler->prefix[handler->matchPos] == '\0')
      {
        // -- prefix matches with the string
        this->resetAllMatches();
        return handler;
      }
    }
    handler = handler->nextHandler;
  }
  return NULL;
}

/**
 * Resets all handlers to match-start position.
 */
void GsmAsync::resetAllMatches()
{
  GsmHandler* handler = this->_firstHandler;
  while(handler != NULL)
  {
    handler->matchPos = 0;
    handler = handler->nextHandler;
  }
}

void GsmAsync::handleOk()
{
  if (this->_nextCommand > 0)
  {
    for(int i=1; i < this->_nextCommand; i++)
    {
      this->_commands[i-1] = this->_commands[i];
      this->_commandTimeouts[i-1] = this->_commandTimeouts[i];
    }
    this->_nextCommand -= 1;
  }
  this->_waitingForResponse = false;
  this->_retryCount = 0;
#if GSMASYNC_DEBUG >= 2
  Serial.print((double)millis() / 1000);
  Serial.println(" GSM OK");
#endif
  this->executeNextCommand();
}

void GsmAsync::handleError()
{
#if GSMASYNC_DEBUG >= 2
  Serial.print((double)millis() / 1000);
  Serial.println(" GSM ERROR");
#endif
  if (this->_errorHandler)
  {
    this->_errorHandler();
  }
}

void GsmAsync::addCommand(const char* command, unsigned long timeOutMs)
{
  if (this->_nextCommand >= GSMASYNC_COMMAND_BUF_SIZE)
  {
    // TODO: generate error
    return;
  }
  this->_commands[this->_nextCommand] = command;
  this->_commandTimeouts[this->_nextCommand] = timeOutMs;
#if GSMASYNC_DEBUG >= 1
  Serial.print((double)millis() / 1000);
  Serial.print(" GSM Command[");
  Serial.print(this->_nextCommand);
  Serial.print("] added: ");
  Serial.println(command);
#endif
  this->_nextCommand += 1;
  if (this->_nextCommand == 1)
  {
    // -- If this is the first command, immediatelly execute it.
    executeNextCommand();
  }
}

void GsmAsync::executeNextCommand()
{
  if (this->_nextCommand > 0)
  {
    const char* currentCommand = this->_commands[0];
#ifdef GSMASYNC_DEBUG
    Serial.print((double)millis() / 1000);
    Serial.print(" GSM CMD: ");
    Serial.println(currentCommand);
#endif
    this->_gsm->println(currentCommand);
    this->_waitingForResponse = true;
    this->_lastSendTime = millis();
  }
}

void GsmAsync::checkTimeout()
{
  if (this->_waitingForResponse)
  {
    unsigned long currentCommandTimeout = this->_commandTimeouts[0];
    if (currentCommandTimeout < (millis() - this->_lastSendTime))
    {
      // -- Time Out
#if GSMASYNC_DEBUG >= 1
      Serial.print((double)millis() / 1000);
      Serial.print(" Timeout for '");
      Serial.print(this->_commands[0]);
      Serial.print("' (");
      Serial.print(currentCommandTimeout);
      Serial.println(" ms).");
#endif
      this->_retryCount += 1;
      if (this->_retryCount >= GSMASYNC_MAX_RETRIES)
      {
        // -- Retries exceeded
#if GSMASYNC_DEBUG >= 1
      Serial.println("Retries exceeded.");
#endif
        // -- Clear commands
        this->clearCommandQueue();
        if (this->_timeoutHandler != NULL)
        {
          this->_timeoutHandler(); // -- Call handler
        }
        return;
      }
      this->executeNextCommand();
    }
  }
}

void GsmAsync::clearCommandQueue()
{
  this->_nextCommand = 0;
  this->_retryCount = 0;
  this->_waitingForResponse = false;
}

