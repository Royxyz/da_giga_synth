#include "TuringMachine.h"

TuringMachine::TuringMachine() {
    shiftRegister = random(0, 65535);
}

void TuringMachine::advanceStep(int probability, int length, bool forceMutate) {
    bool lastBit = (shiftRegister >> (16 - length)) & 1;

    if (forceMutate) {
        lastBit = random(2);
        shiftRegister ^= (1 << random(length)); 
    } else if (random(256) < probability) {
        lastBit = !lastBit; 
    }

    shiftRegister >>= 1;
    if (lastBit) shiftRegister |= (1 << 15);
}

bool TuringMachine::isTrigger() {
    return ((shiftRegister >> 4) & 0x03) != 0; 
}

float TuringMachine::getCurrentMidiNote(int scaleType) {
    uint8_t noteBits = (shiftRegister >> 8) & 0x0F;
    int scaleIndex = map(noteBits, 0, 15, 0, 9);
    
    // UPDATE: Safely wrap around the 7 new scales
    // Absolute value ensures it doesn't break if the encoder goes into negative numbers
    int safeScaleType = abs(scaleType) % 7; 
    
    float baseNote = scales[safeScaleType][scaleIndex];

    uint8_t octBits = (shiftRegister >> 2) & 0x03; 
    int octOffset = (octBits == 3) ? 0 : (octBits - 1);
    
    return baseNote + (octOffset * 12.0f);
}