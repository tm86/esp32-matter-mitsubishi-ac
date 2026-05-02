#include "wifi_manager.h"
#include "build_defaults.generated.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include <esp_sntp.h>

void wifiConnect(const Settings &s) {
    Serial.printf("[WiFi] Connecting to '%s'\n", s.wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(s.deviceName.c_str());
    WiFi.begin(s.wifiSsid.c_str(), s.wifiPass.c_str());

    const unsigned long timeout  = (unsigned long)BUILD_DEFAULT_WIFI_CONNECT_TIMEOUT_SECONDS * 1000UL;
    const unsigned long deadline = millis() + timeout;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(200);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
        if (MDNS.begin(s.deviceName.c_str())) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("[mDNS] Responding as %s.local\n", s.deviceName.c_str());
        } else {
            Serial.println("[mDNS] Failed to start");
        }
        esp_sntp_set_sync_interval((uint32_t)s.ntpRefreshHours * 3600000UL);
        configTzTime(s.ntpTimezone.c_str(), s.ntpServer.c_str());
        Serial.printf("[NTP] Syncing with %s (tz=%s, refresh=%dh)\n",
                      s.ntpServer.c_str(), s.ntpTimezone.c_str(), s.ntpRefreshHours);
        return;
    }

    Serial.println("[WiFi] STA failed.");
    if (!s.apEnabled) return;

    Serial.printf("[WiFi] Starting AP '%s'\n", s.apSsid.c_str());
    WiFi.mode(WIFI_AP);
    WiFi.softAP(s.apSsid.c_str());
    Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}
