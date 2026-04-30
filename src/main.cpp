#include <Arduino.h>
#include "ac_state.h"
#include "settings.h"
#include "wifi_manager.h"
#include "heatpump_task.h"
#include "web_server.h"
#include "mqtt.h"
#include "matter_controller.h"
#include "WebUiHtml.h"

AcState           gAcState;
SemaphoreHandle_t gAcStateMutex = nullptr;

static Settings gSettings;

void setup() {
    Serial.begin(115200);
    delay(100);

    gAcStateMutex = xSemaphoreCreateMutex();
    settingsLoad(gSettings);
    Serial.printf("[main] device: %s\n", gSettings.deviceName.c_str());

    wifiConnect(gSettings);
    webServerStart(gSettings);

    if (gSettings.mqttEnabled) {
        mqttSetup(gSettings);
    }

    matterControllerStart();

    hpTaskStart(
        gSettings.updateInterval,
        /* onStateChanged */ []() {
            wsNotifyState();
            mqttPublishState();
            matterUpdateFromState();
        },
        /* onLog */ [](const LogEntry &e) {
            wsNotifyLog(e);
            mqttPublishLog(e);
        }
    );

    hpLog("info", "ESP-AC starting up...");
    hpLog("info", "HTTP + WebSocket on port 80");
    hpLog("info", "Matter thermostat active");
    if (gSettings.mqttEnabled) {
        hpLog("info", "MQTT enabled: " + gSettings.mqttHost);
    }
}

void loop() {
    if (gPendingRestart && millis() >= gRestartAt) {
        ESP.restart();
    }
    mqttTick();
    webServerTick();
    delay(10);
}
