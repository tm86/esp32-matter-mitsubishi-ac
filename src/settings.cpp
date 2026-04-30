#include "settings.h"
#include "build_defaults.generated.h"
#include <Preferences.h>

static Preferences prefs;

void settingsLoad(Settings &s) {
    prefs.begin("esp-ac", true);
    s.wifiSsid       = prefs.getString("wifi_ssid",   BUILD_DEFAULT_WIFI_SSID);
    s.wifiPass       = prefs.getString("wifi_pass",   BUILD_DEFAULT_WIFI_PASSWORD);
    s.deviceName     = prefs.getString("device_name", BUILD_DEFAULT_DEVICE_NAME);
    s.location       = prefs.getString("location",    BUILD_DEFAULT_LOCATION);
    s.updateInterval = prefs.getInt(   "update_iv",   1);
    s.mqttEnabled    = prefs.getBool(  "mqtt_enabled", BUILD_DEFAULT_MQTT_ENABLED);
    s.mqttHost       = prefs.getString("mqtt_host",   BUILD_DEFAULT_MQTT_HOST);
    s.mqttPort       = prefs.getInt(   "mqtt_port",   BUILD_DEFAULT_MQTT_PORT);
    s.mqttUser       = prefs.getString("mqtt_user",   BUILD_DEFAULT_MQTT_USERNAME);
    s.mqttPass       = prefs.getString("mqtt_pass",   BUILD_DEFAULT_MQTT_PASSWORD);
    s.mqttTopic      = prefs.getString("mqtt_topic",  BUILD_DEFAULT_MQTT_TOPIC_ROOT);
    s.mqttLogs       = prefs.getBool(  "mqtt_logs",   BUILD_DEFAULT_MQTT_LOGS_ENABLED);
    s.mqttHaDisc     = prefs.getBool(  "mqtt_ha_disc", false);
    s.apEnabled      = prefs.getBool(  "ap_enabled",  BUILD_DEFAULT_WEB_AP_FALLBACK_ENABLED);
    s.apSsid         = prefs.getString("ap_ssid",     BUILD_DEFAULT_WEB_AP_SSID);
    prefs.end();
}

void settingsSave(const Settings &s) {
    prefs.begin("esp-ac", false);
    prefs.putString("wifi_ssid",   s.wifiSsid);
    prefs.putString("wifi_pass",   s.wifiPass);
    prefs.putString("device_name", s.deviceName);
    prefs.putString("location",    s.location);
    prefs.putInt(   "update_iv",   s.updateInterval);
    prefs.putBool(  "mqtt_enabled", s.mqttEnabled);
    prefs.putString("mqtt_host",   s.mqttHost);
    prefs.putInt(   "mqtt_port",   s.mqttPort);
    prefs.putString("mqtt_user",   s.mqttUser);
    prefs.putString("mqtt_pass",   s.mqttPass);
    prefs.putString("mqtt_topic",  s.mqttTopic);
    prefs.putBool(  "mqtt_logs",   s.mqttLogs);
    prefs.putBool(  "mqtt_ha_disc", s.mqttHaDisc);
    prefs.putBool(  "ap_enabled",  s.apEnabled);
    prefs.putString("ap_ssid",     s.apSsid);
    prefs.end();
}
