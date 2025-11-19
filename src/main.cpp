#include <Arduino.h>
#include <HeatPump.h>
#include <WiFi.h>

#ifndef HEATPUMP_SERIAL_RX_PIN
#define HEATPUMP_SERIAL_RX_PIN 4
#endif

#ifndef HEATPUMP_SERIAL_TX_PIN
#define HEATPUMP_SERIAL_TX_PIN 5
#endif

#ifndef HEATPUMP_SERIAL_BAUD_RATE
#define HEATPUMP_SERIAL_BAUD_RATE 2400
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 15000
#endif

#ifndef WIFI_RECONNECT_INTERVAL_MS
#define WIFI_RECONNECT_INTERVAL_MS 10000
#endif

namespace {

constexpr uint32_t kLogBaudRate = 115200;
constexpr uint32_t kMatterHeartbeatMs = 5000;

HeatPump heatPump;
HardwareSerial &heatPumpSerial = Serial1;
bool heatPumpConnected = false;
unsigned long lastWifiAttempt = 0;

class MatterBridge {
 public:
  void begin() {
    if (initialized_) {
      return;
    }

    Serial.println("Initializing Matter bridge (stub)");
    Serial.println("Registering Mitsubishi heat pump endpoints for power, mode, and temperature.");
    initialized_ = true;
    lastHeartbeat_ = millis();
  }

  void loop() {
    if (!initialized_ || WiFi.status() != WL_CONNECTED) {
      return;
    }

    const unsigned long now = millis();
    if (now - lastHeartbeat_ >= kMatterHeartbeatMs) {
      Serial.println("Matter bridge heartbeat: ready for commissioning commands over Wi-Fi.");
      lastHeartbeat_ = now;
    }
  }

 private:
  bool initialized_ = false;
  unsigned long lastHeartbeat_ = 0;
};

MatterBridge matterBridge;

void connectWifi() {
  Serial.printf("Connecting to Wi-Fi SSID \"%s\" ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi connected, IP address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Failed to connect to Wi-Fi. Matter functionality will remain offline until the connection succeeds.");
  }

  lastWifiAttempt = millis();
}

void maintainWifiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastWifiAttempt < WIFI_RECONNECT_INTERVAL_MS) {
    return;
  }

  Serial.println("Wi-Fi disconnected, attempting to reconnect ...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = now;
}

void printHeatPumpTimers(const heatpumpTimers &timers) {
  Serial.printf("  Timer mode: %s\n", timers.mode ? timers.mode : "UNKNOWN");
  Serial.printf("  On timer set/remain: %d/%d minutes\n", timers.onMinutesSet, timers.onMinutesRemaining);
  Serial.printf("  Off timer set/remain: %d/%d minutes\n", timers.offMinutesSet, timers.offMinutesRemaining);
}

void printHeatPumpSettings(const heatpumpSettings &settings) {
  Serial.println("Current heat pump settings:");
  Serial.printf("  Power: %s\n", settings.power ? settings.power : "UNKNOWN");
  Serial.printf("  Mode: %s\n", settings.mode ? settings.mode : "UNKNOWN");
  Serial.printf("  Set temperature: %.1f °C\n", settings.temperature);
  Serial.printf("  Fan speed: %s\n", settings.fan ? settings.fan : "UNKNOWN");
  Serial.printf("  Vane (vertical): %s\n", settings.vane ? settings.vane : "UNKNOWN");
  Serial.printf("  Wide vane (horizontal): %s\n", settings.wideVane ? settings.wideVane : "UNKNOWN");
  Serial.printf("  iSee sensor detected: %s\n", settings.iSee ? "yes" : "no");
}

void printHeatPumpStatus(const heatpumpStatus &status) {
  Serial.println("Heat pump status:");
  Serial.printf("  Room temperature: %.1f °C\n", status.roomTemperature);
  Serial.printf("  Operating: %s\n", status.operating ? "yes" : "no");
  Serial.printf("  Compressor frequency: %d\n", status.compressorFrequency);
  printHeatPumpTimers(status.timers);
}

void onHeatPumpSettingsChanged() {
  printHeatPumpSettings(heatPump.getSettings());
}

void onHeatPumpStatusChanged(heatpumpStatus status) {
  printHeatPumpStatus(status);
}

void onHeatPumpRoomTempChanged(float roomTemperature) {
  Serial.printf("Room temperature changed: %.1f °C\n", roomTemperature);
}

void configureInitialSettings() {
  heatPump.setPowerSetting(true);
  heatPump.setModeSetting("HEAT");
  heatPump.setTemperature(22.0f);
  heatPump.setFanSpeed("AUTO");
  heatPump.setVaneSetting("AUTO");
  heatPump.setWideVaneSetting("|");
  heatPump.enableAutoUpdate();
  heatPump.enableExternalUpdate();

  if (heatPump.update()) {
    Serial.println("Initial settings sent to the heat pump interface.");
  } else {
    Serial.println("Failed to send initial settings to the heat pump interface.");
  }
}

void connectHeatPump() {
  Serial.printf("Connecting to heat pump (RX pin %d, TX pin %d) ...\n", HEATPUMP_SERIAL_RX_PIN, HEATPUMP_SERIAL_TX_PIN);
  heatPumpConnected = heatPump.connect(&heatPumpSerial,
                                       HEATPUMP_SERIAL_BAUD_RATE,
                                       HEATPUMP_SERIAL_RX_PIN,
                                       HEATPUMP_SERIAL_TX_PIN);

  if (!heatPumpConnected) {
    Serial.println("Failed to connect to the heat pump interface. Check wiring and serial configuration.");
    return;
  }

  Serial.println("Successfully connected to the heat pump interface.");
  configureInitialSettings();
}

}  // namespace

void setup() {
  Serial.begin(kLogBaudRate);
  while (!Serial && millis() < 2000) {
    delay(10);
  }

  Serial.println();
  Serial.println("ESP32 Mitsubishi Heat Pump controller demo");

  connectWifi();
  matterBridge.begin();

  heatPump.setSettingsChangedCallback(onHeatPumpSettingsChanged);
  heatPump.setStatusChangedCallback(onHeatPumpStatusChanged);
  heatPump.setRoomTempChangedCallback(onHeatPumpRoomTempChanged);

  connectHeatPump();
}

void loop() {
  maintainWifiConnection();
  matterBridge.loop();

  if (heatPumpConnected) {
    heatPump.sync();
  }

  delay(1000);
}
