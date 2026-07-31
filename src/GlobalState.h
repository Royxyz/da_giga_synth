#pragma once
#include <atomic>

struct SynthState {
    // --- 7 Pots & Encoder (Continuous Floats) ---
    std::atomic<float> morph1;       // 0.0f to 7.0f (Frames)
    std::atomic<float> morph2;       // 0.0f to 7.0f (Frames)
    std::atomic<float> detune2;      // -1.0f to 1.0f (Semitones)
    std::atomic<float> filterCutoff; // 20.0f to 20000.0f (Hz)
    std::atomic<float> filterRes;    // 0.0f to 0.99f (Peak)
    std::atomic<float> modDepth;     // 0.0f to 1.0f
    std::atomic<float> fxMix;        // 0.0f to 1.0f (Dry/Wet)
    std::atomic<float> lfoRate;      // 0.1f to 40.0f (Hz)

    // --- 11 Buttons (Discrete States) ---
    // Oscillators
    std::atomic<int>  osc1Bank;       // 0: Analog, 1: Growl, 2: FM
    std::atomic<int>  osc2Bank;
    std::atomic<int>  osc2OctaveDrop; // 0: Normal, 1: -1 Oct, 2: -2 Oct
    std::atomic<bool> oscSync;        // Hard sync toggle

    // Filter & FX
    std::atomic<int>  filterMode;     // 0: LP, 1: BP, 2: HP
    std::atomic<int>  fxMode;         // 0: Tape, 1: Reverb, 2: Shimmer
    std::atomic<bool> fxFreeze;       // Infinite feedback toggle

    // Modulation
    std::atomic<int>  ampEnvShape;    // 0: Pluck, 1: Keys, 2: Pad
    std::atomic<int>  modEnvShape;    // 0: Perc, 1: Sweep, 2: Riser
    std::atomic<int>  lfoWave;        // 0: Sine, 1: Saw, 2: Square, 3: S&H
    std::atomic<int>  modTarget;      // 0: Cutoff, 1: Morph1, 2: Morph2, 3: Pitch
    std::atomic<bool> useModEnv;      // True: Mod Env, False: LFO

    // Constructor to safely initialize atomics on boot
    SynthState(); 
};

extern SynthState state;