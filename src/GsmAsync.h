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

#ifndef GsmAsync_h
#define GsmAsync_h

#include <Arduino.h>

#define GSMASYNC_BUF_SIZE 32
#define GSMASYNC_COMMAND_BUF_SIZE 20
#define GSMASYNC_READ_STATE_NORMAL 0
#define GSMASYNC_READ_STATE_BUFF 1
#define GSMASYNC_REPONSE_TIMEOUT_MS 300
#define GSMASYNC_MAX_RETRIES 3

/**
 * Structure for holding response callbacks.
 */
typedef struct GsmHandler
{
  const char* prefix;
  void (*callback)(char* response);
  byte matchPos; // -- For internal use only
  GsmHandler* nextHandler; // -- For internal use only
} GsmHandler;

/**
 * Main class of the module.
 */
class GsmAsync
{
  public:
    GsmAsync();
    /**
     * Initialize the GsmAsync instance.
     * gsm - Reference to the serail line
     * timeoutHandler - This method will be called, when a command is sent GSMASYNC_MAX_RETRIES times 
     *     and still still no OK answer arrived within timeOutMs from the module.
     * errorHandler - This method will be called, when ERROR response arrives from the module.
     */
    void init(Stream* gsm, void (*timeoutHandler)() = NULL, void (*errorHandler)() = NULL);

    /**
     * The callback will be called, when string match found.
     */
    void registerHandler(GsmHandler* handler);

    /**
     * Add a new command to the command-queue.
     * If the queue was previously empty, the command will be immediatelly executed.
     */
    void addCommand(const char* command, unsigned long timeOutMs = GSMASYNC_REPONSE_TIMEOUT_MS);

    /**
     * Call this as often as possible, to check for new incoming data, and timeouts.
     */
    void doLoop();

  private:
    boolean fillResultBuffer();
    GsmHandler* checkGsmHandler(char c);
    boolean checkOk(char c);
    boolean checkError(char c);
    void resetAllMatches();
    void executeNextCommand();
    void checkTimeout();
    void handleOk();
    void handleError();
    
    Stream* _gsm;
    void (*_timeoutHandler)();
    void (*_errorHandler)();
    char _buffer[GSMASYNC_BUF_SIZE];
    GsmHandler* _firstHandler = NULL;
    GsmHandler* _handlerToCall;
    const char* _commands[GSMASYNC_COMMAND_BUF_SIZE];
    unsigned long _commandTimeouts[GSMASYNC_COMMAND_BUF_SIZE];
    byte _buffPos = 0;
    byte _gsmState = GSMASYNC_READ_STATE_NORMAL;
    size_t _okPos = -1;
    size_t _errorPos = -1;
    byte _nextCommand;
    boolean _waitingForResponse = false;
    unsigned long _lastSendTime;
    byte _retryCount = 0;
};

#endif

