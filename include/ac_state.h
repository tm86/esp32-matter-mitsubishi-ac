#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct AcState {
    bool   power       = false;
    float  temp        = 22.0f;
    String mode        = "COOL";
    String fan         = "AUTO";
    String vane        = "AUTO";
    float  roomTemp    = 0.0f;
    bool   hpConnected = false;
};

extern AcState           gAcState;
extern SemaphoreHandle_t gAcStateMutex;
