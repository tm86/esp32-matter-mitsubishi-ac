#pragma once
#include "settings.h"
#include "heatpump_task.h"

void webServerStart(Settings &settings);
void webServerTick();
void wsNotifyState();
void wsNotifyLog(const LogEntry &e);

extern bool          gPendingRestart;
extern unsigned long gRestartAt;
