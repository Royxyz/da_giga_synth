#pragma once
#include <Arduino.h>

struct SynthState {
    // Live Potentiometer Values (0 - 4095)
    volatile int fm_timbre = 0;   
    volatile int fm_color = 0;    
    volatile int env_shape = 0;   
    volatile int filter_cutoff = 4095; 
    volatile int gran_density = 0;   // Pot 5 (0-4095)
    volatile int gran_position = 0;  // Pot 6 (0-4095)
    volatile int engine_mix = 0;     // Pot 7 (0-4095)
    // Note Data
    volatile uint8_t active_note = 60;
    volatile uint8_t active_velocity = 0;

    

    // Loaded Preset DNA (Loaded on boot)
    volatile uint8_t algorithm = 1;
    volatile float op2_ratio = 1.0f;
    volatile float op3_ratio = 3.0f;
    volatile float op4_ratio = 5.0f;
    volatile float max_mod_index = 450.0f;
};

extern SynthState globalState;