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

    // Initialize Mozzi hardware and parameters here (runs on Core 1 by default)
    setupAudioEngine();
}

void loop() {
    // FIX: Core 1 executes the wrapper naturally. 
    // FreeRTOS background processes on Core 1 will automatically feed the TWDT
    // after each loop iteration, eliminating the need for vTaskDelete and infinite for(;;) starvation.
    audioLoopWrapper(); 
}