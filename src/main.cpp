#include <Arduino.h>
#include "AudioEngine.h"
#include "UI.h"
#include "DummySequencer.h"

TaskHandle_t uiTaskHandle;
TaskHandle_t midiTaskHandle;


DummySequencer sequencer;


void midiTask(void *pvParameters) {
    for(;;) {
        sequencer.update();
        vTaskDelay(pdMS_TO_TICKS(2)); 
    }
}

void setup() {
    Serial.begin(115200);

    sequencer.setCallbacks(engineNoteOn, engineNoteOff);
    sequencer.begin();

    xTaskCreatePinnedToCore(
        uiTask, "UI_Task", 4096, NULL, 1, &uiTaskHandle, 0
    );
    
    xTaskCreatePinnedToCore(
        midiTask, "MIDI_Task", 2048, NULL, 2, &midiTaskHandle, 0
    );

    setupAudioEngine();
}

void loop() {
    audioLoopWrapper(); 
}