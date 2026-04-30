#pragma once
#include <Arduino.h>

struct Settings {
    String wifiSsid;
    String wifiPass;
    String deviceName;
    String location;
    int    updateInterval;
    bool   mqttEnabled;
    String mqttHost;
    int    mqttPort;
    String mqttUser;
    String mqttPass;
    String mqttTopic;
    bool   mqttLogs;
    bool   mqttHaDisc;
    bool   apEnabled;
    String apSsid;
};

void settingsLoad(Settings &s);
void settingsSave(const Settings &s);
