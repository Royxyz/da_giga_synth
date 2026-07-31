#include "GlobalState.h"

SynthState::SynthState() 
    : morph1(0.0f), morph2(0.0f), detune2(0.0f), 
      filterCutoff(2000.0f), filterRes(0.1f), modDepth(0.0f), 
      fxMix(0.0f), lfoRate(1.5f), 
      
      osc1Bank(0), osc2Bank(0), osc2OctaveDrop(0), oscSync(false), 
      
      filterMode(0), fxMode(0), fxFreeze(false), 
      
      ampEnvShape(0), modEnvShape(0), lfoWave(0), 
      modTarget(0), useModEnv(false) 
{}

SynthState state;