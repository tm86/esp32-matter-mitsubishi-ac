#include "matter_controller.h"
#include "ac_state.h"
#include <Matter.h>
#include <MatterEndpoints/MatterThermostat.h>
#include <ArduinoJson.h>

static MatterThermostat sThermostat;

static constexpr float kTempMin = 16.0f;
static constexpr float kTempMax = 31.0f;

static float clampTemp(double v) {
    if (v < kTempMin) return kTempMin;
    if (v > kTempMax) return kTempMax;
    return (float)v;
}

// Translate HP mode string + power flag → MatterThermostat mode enum
static MatterThermostat::ThermostatMode_t acModeToMatter(const String &mode, bool power) {
    if (!power) return MatterThermostat::THERMOSTAT_MODE_OFF;
    if (mode == "HEAT") return MatterThermostat::THERMOSTAT_MODE_HEAT;
    if (mode == "COOL") return MatterThermostat::THERMOSTAT_MODE_COOL;
    if (mode == "DRY")  return MatterThermostat::THERMOSTAT_MODE_DRY;
    if (mode == "FAN")  return MatterThermostat::THERMOSTAT_MODE_FAN_ONLY;
    return MatterThermostat::THERMOSTAT_MODE_AUTO; // AUTO
}

void matterControllerStart() {
    // THERMOSTAT_SEQ_OP_COOLING_HEATING: supports both cool and heat modes.
    // THERMOSTAT_AUTO_MODE_ENABLED: supports auto mode.
    sThermostat.begin(
        MatterThermostat::THERMOSTAT_SEQ_OP_COOLING_HEATING,
        MatterThermostat::THERMOSTAT_AUTO_MODE_ENABLED
    );

    // Matter controller changed the mode (power on/off or mode switch)
    sThermostat.onChangeMode([](MatterThermostat::ThermostatMode_t newMode) -> bool {
        AcCommand cmd = {};
        if (newMode == MatterThermostat::THERMOSTAT_MODE_OFF) {
            cmd.powerSet = true;
            cmd.power    = false;
        } else {
            cmd.powerSet = true;
            cmd.power    = true;
            cmd.modeSet  = true;
            if      (newMode == MatterThermostat::THERMOSTAT_MODE_HEAT)     strlcpy(cmd.mode, "HEAT",  sizeof(cmd.mode));
            else if (newMode == MatterThermostat::THERMOSTAT_MODE_COOL)     strlcpy(cmd.mode, "COOL",  sizeof(cmd.mode));
            else if (newMode == MatterThermostat::THERMOSTAT_MODE_DRY)      strlcpy(cmd.mode, "DRY",   sizeof(cmd.mode));
            else if (newMode == MatterThermostat::THERMOSTAT_MODE_FAN_ONLY) strlcpy(cmd.mode, "FAN",   sizeof(cmd.mode));
            else                                                             strlcpy(cmd.mode, "AUTO",  sizeof(cmd.mode));
        }
        hpSendCommand(cmd);
        return true;
    });

    // Matter controller changed cooling setpoint
    sThermostat.onChangeCoolingSetpoint([](double setpoint) -> bool {
        if (xSemaphoreTake(gAcStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            String mode = gAcState.mode;
            bool power  = gAcState.power;
            xSemaphoreGive(gAcStateMutex);
            if (!power || mode == "HEAT") return true;
        } else {
            return true; // mutex timeout: skip command
        }
        AcCommand cmd = {};
        cmd.tempSet = true;
        cmd.temp    = clampTemp(setpoint);
        hpSendCommand(cmd);
        return true;
    });

    // Matter controller changed heating setpoint
    sThermostat.onChangeHeatingSetpoint([](double setpoint) -> bool {
        if (xSemaphoreTake(gAcStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            String mode = gAcState.mode;
            bool power  = gAcState.power;
            xSemaphoreGive(gAcStateMutex);
            if (!power || mode != "HEAT") return true;
        } else {
            return true; // mutex timeout: skip command
        }
        AcCommand cmd = {};
        cmd.tempSet = true;
        cmd.temp    = clampTemp(setpoint);
        hpSendCommand(cmd);
        return true;
    });

    Matter.begin();
    Serial.println("[Matter] started");
    if (!Matter.isDeviceCommissioned()) {
        Serial.printf("[Matter] not commissioned — code: %s\n",
                      Matter.getManualPairingCode().c_str());
    }
}

void matterUpdateFromState() {
    String mode;
    bool   power;
    float  temp, roomTemp;

    if (xSemaphoreTake(gAcStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        power    = gAcState.power;
        temp     = gAcState.temp;
        mode     = gAcState.mode;
        roomTemp = gAcState.roomTemp;
        xSemaphoreGive(gAcStateMutex);
    } else {
        return;
    }

    sThermostat.setMode(acModeToMatter(mode, power));
    sThermostat.setLocalTemperature(roomTemp);

    // Set both setpoints to current temp (clamped to 16–31°C) so the controller sees a consistent value
    float clamped = clampTemp(temp);
    sThermostat.setHeatingSetpoint(clamped);
    sThermostat.setCoolingSetpoint(clamped);
}

String matterGetStatusJson() {
    JsonDocument doc;
    if (Matter.isDeviceCommissioned()) {
        doc["status"]      = "commissioned";
        doc["pairingCode"] = "";
        doc["qrUrl"]       = "";
    } else {
        doc["status"]      = "not_commissioned";
        doc["pairingCode"] = Matter.getManualPairingCode();
        doc["qrUrl"]       = Matter.getOnboardingQRCodeUrl();
    }
    String out;
    serializeJson(doc, out);
    return out;
}
