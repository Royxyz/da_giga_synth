#include <Arduino.h>
#include "AudioEngine.h"
#include "UI.h"
#include "GlobalState.h"

TaskHandle_t uiTaskHandle;
int16_t* delayBuffer = nullptr; // Changed to int16_t

void setup() {
    Serial.begin(115200);

    // Allocate PSRAM as 16-bit integers
    delayBuffer = (int16_t*)ps_malloc(DELAY_BUFFER_SIZE * sizeof(int16_t));
    if (delayBuffer != nullptr) {
        for(int i = 0; i < DELAY_BUFFER_SIZE; i++) delayBuffer[i] = 0;
    } else {
        Serial.println("PSRAM Allocation Failed!");
    }

    analogReadResolution(12);

    initAudioEngine();
    startMozziEngine(); 

    xTaskCreatePinnedToCore(
        uiTask, 
        "UITask", 
        4096, 
        NULL, 
        1, 
        &uiTaskHandle, 
        0
    );
}

void loop() {
    tickAudioEngine();
}