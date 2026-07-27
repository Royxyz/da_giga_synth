#pragma once
#include <Arduino.h>
#include <Preferences.h>


struct SynthPreset {
    char name[16];
    uint8_t algorithm;      
    float op2_ratio;        
    float op3_ratio;
    float op4_ratio;
    float max_mod_index;    
};

class PresetManager {
public:
    PresetManager();
    void begin();
    void loadPreset(uint8_t index, SynthPreset& preset);
    void savePreset(uint8_t index, const SynthPreset& preset);
    void loadDefaults(); 

private:
    Preferences preferences;
    SynthPreset defaultPresets[3]; // Hardcoded defaults
};