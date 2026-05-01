#include "web_server.h"
#include "ac_state.h"
#include "WebUiHtml.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "matter_controller.h"
#include <Matter.h>

bool          gPendingRestart = false;
unsigned long gRestartAt      = 0;

static AsyncWebServer sServer(80);
static AsyncWebSocket sWs("/ws");
static Settings      *sSettings = nullptr;

static void serializeState(JsonDocument &doc) {
    if (xSemaphoreTake(gAcStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        doc["power"]       = gAcState.power;
        doc["temp"]        = gAcState.temp;
        doc["mode"]        = gAcState.mode;
        doc["fan"]         = gAcState.fan;
        doc["vane"]        = gAcState.vane;
        doc["roomTemp"]    = gAcState.roomTemp;
        doc["hpConnected"] = gAcState.hpConnected;
        xSemaphoreGive(gAcStateMutex);
    }
    doc["hostname"]     = sSettings->deviceName;
    doc["ip"]           = WiFi.status() == WL_CONNECTED
                          ? WiFi.localIP().toString()
                          : WiFi.softAPIP().toString();
    doc["wifi"]         = WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : 0;
    doc["location"]     = sSettings->location;
    doc["firmware"]     = FIRMWARE_VERSION;
    doc["matterStatus"] = Matter.isDeviceCommissioned() ? "commissioned" : "not_commissioned";
}

void wsNotifyState() {
    JsonDocument doc;
    doc["type"] = "state";
    serializeState(doc);
    String msg;
    serializeJson(doc, msg);
    sWs.textAll(msg);
}

void wsNotifyLog(const LogEntry &e) {
    JsonDocument doc;
    doc["type"] = "log";
    doc["t"]    = e.t;
    doc["lvl"]  = e.lvl;
    doc["msg"]  = e.msg;
    String msg;
    serializeJson(doc, msg);
    sWs.textAll(msg);
}

static void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client,
                      AwsEventType type, void *, uint8_t *, size_t) {
    if (type != WS_EVT_CONNECT) return;

    if (xSemaphoreTake(gLogMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        for (const auto &e : gLogBuffer) {
            JsonDocument doc;
            doc["type"] = "log";
            doc["t"]    = e.t;
            doc["lvl"]  = e.lvl;
            doc["msg"]  = e.msg;
            String msg;
            serializeJson(doc, msg);
            client->text(msg);
        }
        xSemaphoreGive(gLogMutex);
    }

    JsonDocument doc;
    doc["type"] = "state";
    serializeState(doc);
    String msg;
    serializeJson(doc, msg);
    client->text(msg);
}

static void setupRoutes() {
    sServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        AsyncWebServerResponse *res = req->beginResponse(
            200, "text/html", WEB_UI_HTML_GZ, WEB_UI_HTML_GZ_LEN);
        res->addHeader("Content-Encoding", "gzip");
        res->addHeader("Cache-Control", "no-cache");
        req->send(res);
    });

    sServer.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        serializeState(doc);
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    sServer.on("/api/state", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", R"({"ok":false})");
                return;
            }
            AcCommand cmd = {};
            if (doc["power"].is<bool>())           { cmd.powerSet = true; cmd.power = doc["power"]; }
            if (doc["temp"].is<float>())           { cmd.tempSet  = true; cmd.temp  = doc["temp"]; }
            if (doc["mode"].is<const char*>()) {
                cmd.modeSet = true;
                strlcpy(cmd.mode, doc["mode"].as<const char*>(), sizeof(cmd.mode));
            }
            if (doc["fan"].is<const char*>()) {
                cmd.fanSet = true;
                strlcpy(cmd.fan, doc["fan"].as<const char*>(), sizeof(cmd.fan));
            }
            if (doc["vane"].is<const char*>()) {
                cmd.vaneSet = true;
                strlcpy(cmd.vane, doc["vane"].as<const char*>(), sizeof(cmd.vane));
            }
            hpSendCommand(cmd);
            req->send(200, "application/json", R"({"ok":true})");
        });

    sServer.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        doc["hostname"]       = sSettings->deviceName;
        doc["location"]       = sSettings->location;
        doc["updateInterval"] = sSettings->updateInterval;
        doc["ssid"]           = sSettings->wifiSsid;
        doc["wifiPassword"]   = "";
        doc["mqttEnabled"]    = sSettings->mqttEnabled;
        doc["mqttHost"]       = sSettings->mqttHost;
        doc["mqttPort"]       = sSettings->mqttPort;
        doc["mqttLogin"]      = sSettings->mqttUser;
        doc["mqttPassword"]   = "";
        doc["mqttTopic"]      = sSettings->mqttTopic;
        doc["mqttLogs"]       = sSettings->mqttLogs;
        doc["mqttHaDisc"]     = sSettings->mqttHaDisc;
        doc["apEnabled"]      = sSettings->apEnabled;
        doc["apSsid"]         = sSettings->apSsid;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    sServer.on("/api/settings", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        nullptr,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", R"({"ok":false})");
                return;
            }
            if (doc["hostname"].is<const char*>())
                sSettings->deviceName = doc["hostname"].as<String>();
            if (doc["location"].is<const char*>())
                sSettings->location = doc["location"].as<String>();
            if (doc["updateInterval"].is<int>())
                sSettings->updateInterval = doc["updateInterval"];
            if (doc["ssid"].is<const char*>())
                sSettings->wifiSsid = doc["ssid"].as<String>();
            if (doc["wifiPassword"].is<const char*>()) {
                String p = doc["wifiPassword"].as<String>();
                if (p.length() > 0) sSettings->wifiPass = p;
            }
            if (doc["mqttEnabled"].is<bool>())    sSettings->mqttEnabled = doc["mqttEnabled"];
            if (doc["mqttHost"].is<const char*>()) sSettings->mqttHost   = doc["mqttHost"].as<String>();
            if (doc["mqttPort"].is<int>())         sSettings->mqttPort   = doc["mqttPort"];
            if (doc["mqttLogin"].is<const char*>()) sSettings->mqttUser  = doc["mqttLogin"].as<String>();
            if (doc["mqttPassword"].is<const char*>()) {
                String p = doc["mqttPassword"].as<String>();
                if (p.length() > 0) sSettings->mqttPass = p;
            }
            if (doc["mqttTopic"].is<const char*>())  sSettings->mqttTopic  = doc["mqttTopic"].as<String>();
            if (doc["mqttLogs"].is<bool>())          sSettings->mqttLogs   = doc["mqttLogs"];
            if (doc["mqttHaDisc"].is<bool>())        sSettings->mqttHaDisc = doc["mqttHaDisc"];
            if (doc["apEnabled"].is<bool>())         sSettings->apEnabled  = doc["apEnabled"];
            if (doc["apSsid"].is<const char*>())     sSettings->apSsid     = doc["apSsid"].as<String>();

            settingsSave(*sSettings);
            req->send(200, "application/json", R"({"ok":true,"reboot":true})");
            gPendingRestart = true;
            gRestartAt      = millis() + 1500;
        });

    sServer.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", R"({"ok":true})");
        gPendingRestart = true;
        gRestartAt      = millis() + 500;
    });

    sServer.on("/api/matter/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", matterGetStatusJson());
    });

    sServer.on("/api/matter/decommission", HTTP_POST, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", R"({"ok":true})");
        Matter.decommission();
        gPendingRestart = true;
        gRestartAt      = millis() + 500;
    });
}

void webServerStart(Settings &settings) {
    sSettings = &settings;
    sWs.onEvent(onWsEvent);
    sServer.addHandler(&sWs);
    setupRoutes();
    sServer.begin();
    Serial.println("[Web] HTTP + WS started on port 80");
}

void webServerTick() {
    sWs.cleanupClients();
}
