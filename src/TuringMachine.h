#pragma once
#include <Arduino.h>

class TuringMachine {
public:
    TuringMachine();
    void advanceStep(int probability, int length, bool forceMutate);
    float getCurrentMidiNote(int scaleType);

private:
    uint16_t shiftRegister;
    const float scales[2][10] = {
        { 60, 63, 65, 67, 70, 72, 75, 77, 79, 82 }, // Minor Pentatonic
        { 60, 62, 64, 65, 67, 69, 71, 72, 74, 76 }  // Major
    };
};