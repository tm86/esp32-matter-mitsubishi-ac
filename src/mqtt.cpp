#include "mqtt.h"
#include "ac_state.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

static WiFiClient    sWifi;
static PubSubClient  sMqtt(sWifi);
static const Settings *sCfg       = nullptr;
static unsigned long   sLastHb    = 0;
static unsigned long   sLastRecon = 0;

static String sTopic(const char *suffix) {
    return sCfg->mqttTopic + suffix;
}

static String buildStateJson() {
    JsonDocument doc;
    if (xSemaphoreTake(gAcStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        doc["power"]    = gAcState.power;
        doc["temp"]     = gAcState.temp;
        doc["mode"]     = gAcState.mode;
        doc["fan"]      = gAcState.fan;
        doc["vane"]     = gAcState.vane;
        doc["roomTemp"] = gAcState.roomTemp;
        xSemaphoreGive(gAcStateMutex);
    }
    String out;
    serializeJson(doc, out);
    return out;
}

static void publishHaDiscovery() {
    if (!sCfg->mqttHaDisc) return;

    String statT = sTopic("/state");
    String setT  = sTopic("/set");
    String connT = sTopic("/connected");
    String discT = "homeassistant/climate/" + sCfg->deviceName + "/config";
    String name  = sCfg->location.length() > 0 ? sCfg->location : sCfg->deviceName;

    JsonDocument doc;
    doc["name"]                             = name;
    doc["unique_id"]                        = sCfg->deviceName;
    doc["current_temperature_topic"]        = statT;
    doc["current_temperature_template"]     = "{{ value_json.roomTemp }}";
    doc["temperature_command_topic"]        = setT;
    doc["temperature_state_topic"]          = statT;
    doc["temperature_state_template"]       = "{{ value_json.temp }}";
    doc["mode_command_topic"]               = setT;
    doc["mode_state_topic"]                 = statT;
    doc["mode_state_template"]              = "{{ value_json.mode | lower }}";
    doc["fan_mode_command_topic"]           = setT;
    doc["fan_mode_state_topic"]             = statT;
    doc["fan_mode_state_template"]          = "{{ value_json.fan | lower }}";
    doc["availability_topic"]               = connT;
    doc["payload_available"]                = "true";
    doc["payload_not_available"]            = "false";
    doc["min_temp"]                         = 16;
    doc["max_temp"]                         = 31;
    doc["temp_step"]                        = 1;
    auto modes = doc["modes"].to<JsonArray>();
    for (auto m : {"auto","heat","cool","dry","fan_only","off"}) modes.add(m);
    auto fans = doc["fan_modes"].to<JsonArray>();
    for (auto f : {"auto","quiet","1","2","3","4"}) fans.add(f);

    String payload;
    serializeJson(doc, payload);
    sMqtt.publish(discT.c_str(), payload.c_str(), true);
}

static void onMqttMessage(char *topic, byte *payload, unsigned int len) {
    JsonDocument doc;
    if (deserializeJson(doc, payload, len)) return;

    AcCommand cmd = {};
    if (doc["power"].is<bool>())        { cmd.powerSet = true; cmd.power = doc["power"]; }
    if (doc["temp"].is<float>())        { cmd.tempSet  = true; cmd.temp  = doc["temp"]; }
    if (doc["mode"].is<const char*>())  { cmd.modeSet  = true; strlcpy(cmd.mode, doc["mode"], sizeof(cmd.mode)); }
    if (doc["fan"].is<const char*>())   { cmd.fanSet   = true; strlcpy(cmd.fan,  doc["fan"],  sizeof(cmd.fan)); }
    if (doc["vane"].is<const char*>())  { cmd.vaneSet  = true; strlcpy(cmd.vane, doc["vane"], sizeof(cmd.vane)); }
    hpSendCommand(cmd);
}

static void mqttConnect() {
    String lwt = sTopic("/connected");
    bool ok = sMqtt.connect(
        sCfg->deviceName.c_str(),
        sCfg->mqttUser.length()  ? sCfg->mqttUser.c_str()  : nullptr,
        sCfg->mqttPass.length()  ? sCfg->mqttPass.c_str()  : nullptr,
        lwt.c_str(), 0, true, "false"
    );
    if (!ok) {
        Serial.printf("[MQTT] connect failed, rc=%d\n", sMqtt.state());
        return;
    }
    Serial.println("[MQTT] connected");
    sMqtt.publish(lwt.c_str(), "true", true);
    sMqtt.subscribe(sTopic("/set").c_str());
    publishHaDiscovery();
    mqttPublishState();
}

void mqttSetup(const Settings &s) {
    sCfg = &s;
    sMqtt.setServer(s.mqttHost.c_str(), s.mqttPort);
    sMqtt.setCallback(onMqttMessage);
    sMqtt.setBufferSize(1024);
}

void mqttTick() {
    if (!sCfg || !sCfg->mqttEnabled) return;
    if (!sMqtt.connected()) {
        if (millis() - sLastRecon > 5000) {
            sLastRecon = millis();
            mqttConnect();
        }
        return;
    }
    sMqtt.loop();
    if (millis() - sLastHb > 60000) {
        sLastHb = millis();
        mqttPublishState();
    }
}

void mqttPublishState() {
    if (!sCfg || !sCfg->mqttEnabled || !sMqtt.connected()) return;
    String p = buildStateJson();
    sMqtt.publish(sTopic("/state").c_str(), p.c_str(), false);
}

void mqttPublishLog(const LogEntry &e) {
    if (!sCfg || !sCfg->mqttEnabled || !sCfg->mqttLogs || !sMqtt.connected()) return;
    JsonDocument doc;
    doc["t"]   = e.t;
    doc["lvl"] = e.lvl;
    doc["msg"] = e.msg;
    String p;
    serializeJson(doc, p);
    sMqtt.publish(sTopic("/log").c_str(), p.c_str(), false);
}
