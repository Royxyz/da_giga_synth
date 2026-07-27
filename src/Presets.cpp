#include "Presets.h"

PresetManager::PresetManager() {

    defaultPresets[0] = {"Abyssal Drone", 1, 1.0f, 2.0f, 0.25f, 180.0f}; 
    defaultPresets[1] = {"Glassy Pad",    2, 2.0f, 4.0f, 1.0f, 120.0f};
    defaultPresets[2] = {"Harsh Lead",    1, 1.0f, 3.0f, 7.0f, 400.0f};
}


void PresetManager::begin() {
    preferences.begin("synth", false);
}

void PresetManager::savePreset(uint8_t index, const SynthPreset& preset) {
    String key = "preset_" + String(index);
    preferences.putBytes(key.c_str(), &preset, sizeof(SynthPreset));
}

void PresetManager::loadPreset(uint8_t index, SynthPreset& preset) {
    String key = "preset_" + String(index);
    if (preferences.isKey(key.c_str())) {
        preferences.getBytes(key.c_str(), &preset, sizeof(SynthPreset));
    } else {
        preset = defaultPresets[index % 3]; 
    }
}