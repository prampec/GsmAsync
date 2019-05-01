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

static char* GSMASYNC_OK_STR = "OK";
static char* GSMASYNC_ERROR_STR = "ERROR";

GsmAsync::GsmAsync(Stream* gsm, void (*timeoutHandler)(), void (*errorHandler)())
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
      /*
      Serial.println();
      Serial.print("Calling handler for '");
      Serial.print(this->_handlerToCall->prefix);
      Serial.print("' with argument '");
      Serial.print(this->_buffer);
      Serial.println("'.");
      */
      if (bufferReady)
      {
        (this->_handlerToCall->callback)(this->_buffer); // -- Call callback.
        this->_gsmState = GSMASYNC_READ_STATE_NORMAL;
      }
    }
    else // -- GSMASYNC_READ_STATE_NORMAL
    {
      // -- Read all characters and search for a pattern match.
      char c = this->_gsm->read();
//Serial.write(c);
      if (c < 32)
      {
        this->_okPos = 0;
        this->_errorPos = 0;
        continue;
      }
      if (checkOk(c))
      {
        // TODO: might want to clear read buffer before calling any new command
        this->handleOk();
        continue;
      }
      if (checkError(c))
      {
        // TODO: might want to clear read buffer before calling any new command
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
    if (this->_okPos == strlen(GSMASYNC_OK_STR))
    {
      this->_okPos = 0;
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
    if (this->_errorPos == strlen(GSMASYNC_ERROR_STR))
    {
      this->_errorPos = 0;
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
    if ((this->_buffPos == 0) && (c == ' '))
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
//Serial.println("OK");
  this->executeNextCommand();
}

void GsmAsync::handleError()
{
  if (this->_errorHandler)
  {
    this->_errorHandler();
  }
}

void GsmAsync::addCommand(char* command, unsigned long timeOutMs)
{
  if (this->_nextCommand >= GSMASYNC_COMMAND_BUF_SIZE)
  {
    // TODO: generate error
    return;
  }
  this->_commands[this->_nextCommand] = command;
  this->_commandTimeouts[this->_nextCommand] = timeOutMs;
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
//Serial.println(currentCommand);
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
      this->_retryCount += 1;
      if (this->_retryCount >= GSMASYNC_MAX_RETRIES)
      {
        // -- Retries exceeded
        // -- Clear commands
        this->_nextCommand = 0;
        this->_retryCount = 0;
        this->_waitingForResponse = false;
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

