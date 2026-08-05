#include "GlobalState.h"

QueueHandle_t midiQueue = NULL;

SynthState::SynthState() 
    : osc1Bank(0), osc2Bank(0), osc1BaseMorph(0.0f), osc2BaseMorph(0.0f), 
      osc1Coarse(0.0f), osc2Coarse(0.0f), oscMix(0.5f),
      lfo1Rate(1.5f), lfo1Wave(0), lfo2Rate(1.5f), lfo2Wave(0),
      filterCutoff(2000.0f), filterRes(0.1f), filterMode(0),
      fxFreeze(false), fxMode(0), abyssSend(0.0f)
{
    // --- 1. Zero Out the Modulation Matrix ---
    for (int i = 0; i < 30; i++) {
        modMatrix[i].store(0.0f);
    }

    // --- 2. Initialize Default Envelopes (A, D, S, R) ---
    // Amp Env: 10ms Attack, 100ms Decay, 0.8 Sustain, 200ms Release
    ampEnv[0].store(10.0f);   ampEnv[1].store(100.0f); 
    ampEnv[2].store(0.8f);    ampEnv[3].store(200.0f);

    // Mod Env 1: Plucky (10ms A, 100ms D, 0.0 S, 100ms R)
    modEnv1[0].store(10.0f);  modEnv1[1].store(100.0f); 
    modEnv1[2].store(0.0f);   modEnv1[3].store(100.0f);

    // Mod Env 2: Plucky
    modEnv2[0].store(10.0f);  modEnv2[1].store(100.0f); 
    modEnv2[2].store(0.0f);   modEnv2[3].store(100.0f);
}

SynthState state;
