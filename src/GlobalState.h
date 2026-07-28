#pragma once

struct SynthState {
    volatile int tempo;           
    volatile int probability;     
    volatile int sequenceLength;  
    volatile int glide;           
    volatile int filterCutoff;    
    volatile int filterRes;       
    volatile int envDecay;        
    
    volatile int scaleType;       
    volatile bool lockSequence;   
    volatile bool forceMutate;
    volatile int octaveOffset;
    
    // 0 = Bypass, 1 = Wash, 2 = Unison, 3 = Rave Chord
    volatile int activeFX; 
};

extern SynthState state;