#include <Arduino.h>
#include "AudioEngine.h"
#include "UI.h"
#include "DummySequencer.h"

TaskHandle_t uiTaskHandle;
TaskHandle_t midiTaskHandle;
TaskHandle_t audioTaskHandle; // <--- ADDED

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

    // Grouping all the UI and MIDI on Core 0!
    xTaskCreatePinnedToCore(uiTask, "UI_Task", 4096, NULL, 1, &uiTaskHandle, 0);
    xTaskCreatePinnedToCore(midiTask, "MIDI_Task", 2048, NULL, 2, &midiTaskHandle, 0);

    // Audio Engine gets Core 1 entirely to itself.
    xTaskCreatePinnedToCore(audioTask, "Audio_Task", 8192, NULL, 3, &audioTaskHandle, 1);
}

void loop() {
    // Delete the default Arduino loop task so it doesn't waste CPU cycles
    vTaskDelete(NULL); 
}