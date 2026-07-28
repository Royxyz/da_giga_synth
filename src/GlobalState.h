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
};

extern SynthState state;