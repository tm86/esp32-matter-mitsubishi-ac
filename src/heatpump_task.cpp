#include "heatpump_task.h"
#include "ac_state.h"
#include <HeatPump.h>
#include <Arduino.h>
#include <time.h>
#include <math.h>

std::deque<LogEntry> gLogBuffer;
SemaphoreHandle_t    gLogMutex    = nullptr;

static HeatPump       sHp;
static QueueHandle_t  sCmdQueue   = nullptr;
static StateChangedCb sOnState;
static LogCb          sOnLog;
static int            sIntervalSec = 1;

void hpLog(const String &lvl, const String &msg) {
    char t[9];
    time_t now = time(nullptr);
    if (now > 1000000000L) {
        struct tm *ti = localtime(&now);
        strftime(t, sizeof(t), "%H:%M:%S", ti);
    } else {
        unsigned long s = millis() / 1000;
        snprintf(t, sizeof(t), "%02lu:%02lu:%02lu", (s / 3600) % 24, (s / 60) % 60, s % 60);
    }
    LogEntry e{t, lvl, msg};

    if (xSemaphoreTake(gLogMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        gLogBuffer.push_back(e);
        if (gLogBuffer.size() > 200) gLogBuffer.pop_front();
        xSemaphoreGive(gLogMutex);
    }
    if (sOnLog) sOnLog(e);
    Serial.printf("[%s] %s\n", lvl.c_str(), msg.c_str());
}

// HeatPump library uses no-arg callbacks; read back from sHp after firing
static void onSettingsChanged() {
    heatpumpSettings s = sHp.getSettings();
    if (xSemaphoreTake(gAcStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        gAcState.power = (strcmp(s.power, "ON") == 0);
        gAcState.temp  = s.temperature;
        gAcState.mode  = s.mode;
        gAcState.fan   = s.fan;
        gAcState.vane  = s.vane;
        xSemaphoreGive(gAcStateMutex);
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "AC settings: pwr=%s temp=%.1f mode=%s fan=%s vane=%s",
             s.power, s.temperature, s.mode, s.fan, s.vane);
    hpLog("info", buf);
    if (sOnState) sOnState();
}

static void onStatusChanged(heatpumpStatus s) {
    if (xSemaphoreTake(gAcStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        gAcState.roomTemp = s.roomTemperature;
        xSemaphoreGive(gAcStateMutex);
    }
    char buf[48];
    snprintf(buf, sizeof(buf), "AC status: roomTemp=%.1f°C operating=%s",
             s.roomTemperature, s.operating ? "yes" : "no");
    hpLog("info", buf);
    if (sOnState) sOnState();
}

static void hpTaskFn(void *) {
    hpLog("info", "HP task started");
    Serial1.begin(2400, SERIAL_8E1, 20, 21); // RX=GPIO20(D7), TX=GPIO21(D6)
    sHp.setSettingsChangedCallback(onSettingsChanged);
    sHp.setStatusChangedCallback(onStatusChanged);
    sHp.connect(&Serial1);

    {
        bool connected = sHp.isConnected();
        if (xSemaphoreTake(gAcStateMutex, portMAX_DELAY) == pdTRUE) {
            gAcState.hpConnected = connected;
            xSemaphoreGive(gAcStateMutex);
        }
        hpLog(connected ? "info" : "warn",
              connected ? "Heat pump connected" : "Heat pump not responding — check CN105 wiring");
    }

    TickType_t lastWake = xTaskGetTickCount();
    bool prevConnected = sHp.isConnected();

    for (;;) {
        AcCommand cmd = {};
        if (xQueueReceive(sCmdQueue, &cmd, 0) == pdTRUE) {
            if (cmd.powerSet) sHp.setPowerSetting(cmd.power ? "ON" : "OFF");
            if (cmd.tempSet)  sHp.setTemperature(cmd.temp);
            if (cmd.modeSet)  sHp.setModeSetting(cmd.mode);
            if (cmd.fanSet)   sHp.setFanSpeed(cmd.fan);
            if (cmd.vaneSet)  sHp.setVaneSetting(cmd.vane);
            sHp.update();
            {
                String logMsg = "Cmd sent:";
                if (cmd.powerSet) logMsg += " pwr=" + String(cmd.power ? "ON" : "OFF");
                if (cmd.tempSet)  { char tb[12]; snprintf(tb, sizeof(tb), " temp=%.1f", cmd.temp); logMsg += tb; }
                if (cmd.modeSet)  logMsg += " mode=" + String(cmd.mode);
                if (cmd.fanSet)   logMsg += " fan=" + String(cmd.fan);
                if (cmd.vaneSet)  logMsg += " vane=" + String(cmd.vane);
                hpLog("info", logMsg);
            }
        }

        sHp.sync();

        bool nowConnected = sHp.isConnected();
        if (xSemaphoreTake(gAcStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            gAcState.hpConnected = nowConnected;
            xSemaphoreGive(gAcStateMutex);
        }
        if (nowConnected && !prevConnected) hpLog("info", "Heat pump reconnected");
        if (!nowConnected && prevConnected) hpLog("warn", "Heat pump disconnected");
        prevConnected = nowConnected;

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(sIntervalSec * 1000));
    }
}

void hpTaskStart(int updateIntervalSec, StateChangedCb onState, LogCb onLog) {
    sIntervalSec = updateIntervalSec;
    sOnState     = onState;
    sOnLog       = onLog;
    gLogMutex    = xSemaphoreCreateMutex();
    sCmdQueue    = xQueueCreate(4, sizeof(AcCommand));
    xTaskCreate(hpTaskFn, "heatpump", 4096, nullptr, 3, nullptr);
}

void hpSendCommand(const AcCommand &cmd) {
    xQueueSend(sCmdQueue, &cmd, pdMS_TO_TICKS(50));
}
