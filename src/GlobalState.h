#pragma once
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// --- Thread-Safe MIDI Queue Structure ---
struct MidiEvent { 
    uint8_t type; 
    uint8_t note; 
};

extern QueueHandle_t midiQueue;

// --- Global UI Parameters ---
struct SynthState {
    // --- 7 Pots & Encoder (Continuous Floats) ---
    std::atomic<float> morph1;       // 0.0f to 127.0f (Frames - Upgraded for 128 slices)
    std::atomic<float> envAttack;    // 1.0f to 2000.0f (ms)
    std::atomic<float> envRelease;   // 1.0f to 2000.0f (ms)
    std::atomic<float> filterCutoff; // 20.0f to 20000.0f (Hz)
    std::atomic<float> filterRes;    // 0.0f to 0.99f (Peak)
    std::atomic<float> modDepth;     // 0.0f to 1.0f
    std::atomic<float> fxMix;        // 0.0f to 1.0f (Dry/Wet)
    std::atomic<float> lfoRate;      // 0.1f to 40.0f (Hz)

    // --- 7 Active Buttons (Discrete States) ---
    std::atomic<int>  osc1Bank;       // 0: Analog, 1: Growl, 2: FM
    std::atomic<int>  filterMode;     // 0: LP, 1: BP, 2: HP
    std::atomic<int>  fxMode;         // 0: Tape, 1: Reverb, 2: Shimmer
    std::atomic<bool> fxFreeze;       // Infinite feedback toggle
    std::atomic<int>  modEnvShape;    // 0: Perc, 1: Sweep, 2: Riser
    std::atomic<int>  lfoWave;        // 0: Sine, 1: Saw, 2: Square, 3: S&H
    std::atomic<int>  modTarget;      // 0: Cutoff, 1: Morph1, 2: Pitch
    std::atomic<bool> useModEnv;      // True: Mod Env, False: LFO

    SynthState(); 
};

extern SynthState state;