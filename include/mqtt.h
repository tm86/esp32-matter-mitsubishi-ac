#pragma once
#include "settings.h"
#include "heatpump_task.h"

void mqttSetup(const Settings &s);
void mqttTick();
void mqttPublishState();
void mqttPublishLog(const LogEntry &e);
