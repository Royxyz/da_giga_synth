#pragma once
#include <Arduino.h>

class TuringMachine {
public:
    TuringMachine();
    void advanceStep(int probability, int length, bool forceMutate);
    float getCurrentMidiNote(int scaleType);
    bool isTrigger(); 

private:
    uint16_t shiftRegister;
    
    // Upgraded to 7 distinct scales (10 notes each for wide melodic range)
    const float scales[7][10] = {
        { 60, 63, 65, 67, 70, 72, 75, 77, 79, 82 }, // 0: Minor Pentatonic (Classic, moody electronic)
        { 60, 62, 64, 65, 67, 69, 71, 72, 74, 76 }, // 1: Major (Bright, happy, uplifting)
        { 60, 62, 63, 65, 67, 68, 71, 72, 74, 75 }, // 2: Harmonic Minor (Dark, intense, neo-classical)
        { 60, 61, 64, 65, 67, 68, 70, 72, 73, 76 }, // 3: Phrygian Dominant (Exotic, Middle Eastern/desert vibes)
        { 60, 62, 63, 65, 67, 69, 70, 72, 74, 75 }, // 4: Dorian (Jazzy, melancholy but hopeful)
        { 60, 62, 63, 67, 68, 72, 74, 75, 79, 80 }, // 5: Hirajoshi (Ethereal, Japanese pentatonic)
        { 60, 62, 64, 66, 68, 70, 72, 74, 76, 78 }  // 6: Whole Tone (Dreamy, floating, unresolved)
    };
};