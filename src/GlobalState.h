#pragma once
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct MidiEvent { 
    uint8_t type; 
    uint8_t note; 
    uint8_t velocity; // Added velocity tracking
};

extern QueueHandle_t midiQueue;

struct SynthState {
    // --- The Modulation Matrix (6 Sources x 5 Destinations = 30 Nodes) ---
    // SOURCES: 0=Velocity, 1=AmpEnv, 2=ModEnv1, 3=ModEnv2, 4=LFO1(Global), 5=LFO2(Poly)
    // DESTINATIONS: 0=Amp, 1=Pitch, 2=Osc1Pos, 3=Osc2Pos, 4=Cutoff
    // Index = (Source * 5) + Destination
    std::atomic<float> modMatrix[30];

    // --- Oscillators ---
    std::atomic<int>   osc1Bank;
    std::atomic<int>   osc2Bank;
    std::atomic<float> osc1BaseMorph; // 0 to 128.0f
    std::atomic<float> osc2BaseMorph; // 0 to 128.0f
    std::atomic<float> osc1Coarse;    // Semitones
    std::atomic<float> osc2Coarse;    // Semitones
    std::atomic<float> oscMix;        // 0.0f (100% Osc1) to 1.0f (100% Osc2)

    // --- Envelopes (A, D, S, R) ---
    std::atomic<float> ampEnv[4];
    std::atomic<float> modEnv1[4];
    std::atomic<float> modEnv2[4];

    // --- LFOs ---
    std::atomic<float> lfo1Rate; // Global
    std::atomic<int>   lfo1Wave;
    std::atomic<float> lfo2Rate; // Polyphonic
    std::atomic<int>   lfo2Wave;

    // --- Filter ---
    std::atomic<float> filterCutoff; // Base Hz
    std::atomic<float> filterRes;
    std::atomic<int>   filterMode;   // 0: LP, 1: BP, 2: HP
    
    // --- FX (Kept minimal for the Abyss) ---
    std::atomic<bool> fxFreeze;       
    std::atomic<int>  fxMode;         
    std::atomic<float> abyssSend;

    SynthState(); 
};

extern SynthState state;