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
    
    // 0 = No FX, 1 = Wash Cloud (Reserving 2 and 3 for future buttons)
    volatile int activeFX;        
};

extern SynthState state;