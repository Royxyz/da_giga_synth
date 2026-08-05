#include "GlobalState.h"

// Instantiate the FreeRTOS Queue Handle
QueueHandle_t midiQueue = NULL;

SynthState::SynthState() 
    : morph1(0.0f), envAttack(10.0f), envRelease(200.0f), 
      filterCutoff(2000.0f), filterRes(0.1f), modDepth(0.0f), 
      fxMix(0.0f), lfoRate(1.5f), 
      
      osc1Bank(0), filterMode(0), fxMode(0), fxFreeze(false), 
      
      modEnvShape(0), lfoWave(0), modTarget(0), useModEnv(false) 
{}

SynthState state;