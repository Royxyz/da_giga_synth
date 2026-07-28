#include "TuringMachine.h"

TuringMachine::TuringMachine() {
    shiftRegister = random(0, 65535);
}

void TuringMachine::advanceStep(int probability, int length, bool forceMutate) {
    bool lastBit = (shiftRegister >> (16 - length)) & 1;

    if (forceMutate || random(256) < probability) {
        lastBit = !lastBit; 
    }

    shiftRegister >>= 1;
    if (lastBit) shiftRegister |= (1 << 15);
}

float TuringMachine::getCurrentMidiNote(int scaleType) {
    uint8_t rawValue = (shiftRegister >> 12) & 0x0F;
    int scaleIndex = map(rawValue, 0, 15, 0, 9);
    int safeScaleType = scaleType % 2; 
    
    return scales[safeScaleType][scaleIndex];
}