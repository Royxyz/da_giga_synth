#pragma once

struct SynthState {
    // Timbre & Chord Pots
    volatile int osc1Pitch;
    volatile int osc2Pitch;
    volatile int osc3Pitch;
    volatile int wavetableMorph; 
    volatile int filterCutoff;    
    volatile int filterRes;       
    volatile int washMix;        
    
    // Core Engine States
    volatile int rootNote;       
    volatile int activeBank;     
    volatile bool droneMode;     
    volatile bool triggerStab;   

    // --- NEW: Performance Modifiers ---
    volatile bool autoChord;      // Triggers stab when twisting pitch pots
    volatile bool lfoRepeater;    // Trance gate toggle
    volatile int envShape;        // 0: Pluck, 1: Brass, 2: Pad
    volatile int lfoTarget;       // 0: Off, 1: Cutoff, 2: Morph, 3: Pitch
    volatile int glideSpeed;      // 0: Instant, 1: Medium, 2: Slow
    volatile int activeScale;     // 0: Chromatic, 1: Minor, 2: Major, 3: Phrygian
    volatile bool washFreeze;     // 100% delay feedback
    volatile bool subBassDrop;    // Drop Osc 1 by 2 octaves
};

extern SynthState state;