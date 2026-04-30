#pragma once
#include "heatpump_task.h"

// Called once from setup() — after wifiConnect() and webServerStart().
void matterControllerStart();

// Called from the StateChangedCb (main.cpp) — pushes gAcState → Matter attributes.
void matterUpdateFromState();

// Returns current commissioning status as JSON string.
// {"status":"not_commissioned","pairingCode":"XXXXX-XXXXX","qrUrl":"https://..."}
// {"status":"commissioned","pairingCode":"","qrUrl":""}
String matterGetStatusJson();
