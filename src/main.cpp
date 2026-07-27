#include <Arduino.h>
#include "UI.h"
#include "AudioEngine.h"

TaskHandle_t UI_Task;
TaskHandle_t Audio_Task;

void setup() {
    Serial.begin(115200);

    setupUI();

    xTaskCreatePinnedToCore(
        uiTask,        
        "UI_Task",     
        4096,          
        NULL,          
        1,             
        &UI_Task,      
        0              
    );

    xTaskCreatePinnedToCore(
        audioTask,     
        "Audio_Task",  
        8192,          
        NULL,          
        configMAX_PRIORITIES - 1, 
        &Audio_Task,   
        1              
    );
}

void loop() {
    vTaskDelete(NULL); 
}