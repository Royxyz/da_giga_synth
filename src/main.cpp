#include <Arduino.h>
#include "AudioEngine.h"
#include "UI.h"
#include "Sequencer.h" 
#include "GlobalState.h"
#include "SynthNetwork.h"

TaskHandle_t networkTaskHandle;
TaskHandle_t uiTaskHandle;
TaskHandle_t midiTaskHandle;
TaskHandle_t audioTaskHandle; 

Sequencer sequencer; 

void midiTask(void *pvParameters) {
    for(;;) {
        sequencer.update();
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);


    midiQueue = xQueueCreate(32, sizeof(MidiEvent));

    sequencer.setCallbacks(engineNoteOn, engineNoteOff);
    sequencer.begin();

    xTaskCreatePinnedToCore(uiTask, "UI_Task", 4096, NULL, 1, &uiTaskHandle, 0);
    xTaskCreatePinnedToCore(midiTask, "MIDI_Task", 2048, NULL, 2, &midiTaskHandle, 0);

    xTaskCreatePinnedToCore(networkTask, "Network_Task", 8192, NULL, 1, &networkTaskHandle, 0);




    xTaskCreatePinnedToCore(audioTask, "Audio_Task", 8192, NULL, 3, &audioTaskHandle, 1);
}

void loop() {
    vTaskDelete(NULL); 
}