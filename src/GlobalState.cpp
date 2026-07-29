#include "GlobalState.h"

// Instantiate the global state with default boot values
SynthState state = {
    // Pots
    0, 0, 0, 0,         // osc1Pitch, osc2Pitch, osc3Pitch, wavetableMorph
    4095, 0, 0,         // filterCutoff, filterRes, washMix
    
    // Core Engine
    36,                 // rootNote (C2)
    0,                  // activeBank
    true,               // droneMode
    false,              // triggerStab
    
    // Performance Modifiers
    false,              // autoChord
    false,              // lfoRepeater
    0,                  // envShape
    0,                  // lfoTarget
    0,                  // glideSpeed
    0,                  // activeScale
    false,              // washFreeze
    false               // subBassDrop
};