#include <Arduino.h>
#include <HeatPump.h>

#ifndef HEATPUMP_SERIAL_RX_PIN
#define HEATPUMP_SERIAL_RX_PIN 4
#endif

#ifndef HEATPUMP_SERIAL_TX_PIN
#define HEATPUMP_SERIAL_TX_PIN 5
#endif

#ifndef HEATPUMP_SERIAL_BAUD_RATE
#define HEATPUMP_SERIAL_BAUD_RATE 2400
#endif

namespace {

constexpr uint32_t kLogBaudRate = 115200;

HeatPump heatPump;
HardwareSerial &heatPumpSerial = Serial1;
bool heatPumpConnected = false;

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

  heatPump.setSettingsChangedCallback(onHeatPumpSettingsChanged);
  heatPump.setStatusChangedCallback(onHeatPumpStatusChanged);
  heatPump.setRoomTempChangedCallback(onHeatPumpRoomTempChanged);

  connectHeatPump();
}

void loop() {
  if (heatPumpConnected) {
    heatPump.sync();
  }

  delay(1000);
}
