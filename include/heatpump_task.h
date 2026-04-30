#pragma once
#include <Arduino.h>
#include <deque>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

struct LogEntry {
    String t;
    String lvl;
    String msg;
};

struct AcCommand {
    bool  powerSet; bool  power;
    bool  tempSet;  float temp;
    bool  modeSet;  char  mode[8];
    bool  fanSet;   char  fan[8];
    bool  vaneSet;  char  vane[8];
};

extern std::deque<LogEntry> gLogBuffer;
extern SemaphoreHandle_t    gLogMutex;

using StateChangedCb = std::function<void()>;
using LogCb          = std::function<void(const LogEntry &)>;

void hpTaskStart(int updateIntervalSec, StateChangedCb onState, LogCb onLog);
void hpSendCommand(const AcCommand &cmd);
void hpLog(const String &lvl, const String &msg);
