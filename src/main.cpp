#include <Arduino.h>
#include "AudioEngine.h"
#include "UI.h"

TaskHandle_t audioTaskHandle;
TaskHandle_t uiTaskHandle;

void setup() {
    xTaskCreatePinnedToCore(
        uiTask, 
        "UI_Task", 
        4096,           
        NULL,           
        1,              
        &uiTaskHandle, 
        0               
    );

    xTaskCreatePinnedToCore(
        audioTask, 
        "Audio_Task", 
        8192,           
        NULL, 
        configMAX_PRIORITIES - 1, 
        &audioTaskHandle, 
        1               
    );
}

void loop() {
    vTaskDelete(NULL); 
}