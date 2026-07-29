#include <Arduino.h>
#include "AudioEngine.h"
#include "UI.h"

TaskHandle_t uiTaskHandle;

void setup() {
    Serial.begin(115200);

    // Pin UI task to Core 0 
    xTaskCreatePinnedToCore(
        uiTask, "UI_Task", 4096, NULL, 1, &uiTaskHandle, 0
    );

    // Initialize Mozzi hardware and parameters here (runs on Core 1)
    setupAudioEngine();
}

void loop() {
    // Core 1 natively handles the audio loop and feeds the watchdog
    audioLoopWrapper(); 
}