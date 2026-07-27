#include "UI.h"
#include "GlobalState.h"
#include "PatternSequencer.h"
#include "Presets.h"

// Pin definitions (Pots only)
const int PIN_POT_TIMBRE = 7;
const int PIN_POT_COLOR  = 8;
const int PIN_POT_ENV    = 9;
const int PIN_POT_FILTER = 10;
const int PIN_POT_DENSITY  = 11;
const int PIN_POT_POSITION = 12;
const int PIN_POT_MIX      = 13;

INoteSource* noteSource;
PresetManager presetManager;
SynthPreset currentPreset;

void setupUI() {
    // Configure pot pins
    pinMode(PIN_POT_TIMBRE, INPUT);
    pinMode(PIN_POT_COLOR, INPUT);
    pinMode(PIN_POT_ENV, INPUT);
    pinMode(PIN_POT_FILTER, INPUT);
    pinMode(PIN_POT_DENSITY, INPUT);
    pinMode(PIN_POT_POSITION, INPUT);
    pinMode(PIN_POT_MIX, INPUT);
    
    // Initialize NVS storage and load Preset 0 directly
    presetManager.begin();
    presetManager.loadPreset(0, currentPreset);
    
    // Push Preset 0's core DNA into the volatile global state
    globalState.algorithm = currentPreset.algorithm;
    globalState.op2_ratio = currentPreset.op2_ratio;
    globalState.op3_ratio = currentPreset.op3_ratio;
    globalState.op4_ratio = currentPreset.op4_ratio;
    globalState.max_mod_index = currentPreset.max_mod_index;

    // Inject our Pattern Sequencer at 120 BPM
    noteSource = new PatternSequencer(30); 
}

void uiTask(void *pvParameters) {
    // Timer for non-blocking serial prints
    static uint32_t last_print_time = 0;

    for (;;) {
        // 1. Read pots and update global state
        globalState.fm_timbre = analogRead(PIN_POT_TIMBRE); 
        globalState.fm_color = analogRead(PIN_POT_COLOR);
        globalState.env_shape = analogRead(PIN_POT_ENV);
        globalState.filter_cutoff = analogRead(PIN_POT_FILTER);
        globalState.gran_density = analogRead(PIN_POT_DENSITY);
        globalState.gran_position = analogRead(PIN_POT_POSITION);
        globalState.engine_mix = analogRead(PIN_POT_MIX);
        // 2. Process Note Inputs from the Pattern Sequencer
        noteSource->update();
        if (noteSource->hasEvent()) {
            NoteEvent ev = noteSource->popEvent();
            globalState.active_note = ev.note;
            globalState.active_velocity = ev.velocity;
        }

        // 3. --- DIAGNOSTIC PRINT BLOCK ---
        uint32_t now = millis();
        if (now - last_print_time >= 500) {
            last_print_time = now;
            
            Serial.printf("POTS -> Timb: %4d | Col: %4d | Env: %4d | Filt: %4d || SEQ -> Note: %3d | Vel: %3d\n",
                globalState.fm_timbre,
                globalState.fm_color,
                globalState.env_shape,
                globalState.filter_cutoff,
                globalState.active_note,
                globalState.active_velocity
            );
        }

        // Delay to prevent the UI task from tripping the Core 0 watchdog timer
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}