#pragma once
#include <Arduino.h>

class FloatWavetable {
private:
    float phase = 0.0f;
    float phaseInc = 0.0f;
    const int16_t* bank; // 16-bit pointer
    int tableSize;
    int numFrames;

public:
    bool phaseWrapped = false; 

    FloatWavetable(const int16_t* waveBank, int size, int frames) {
        setTable(waveBank, size, frames);
    }

    void setTable(const int16_t* waveBank, int size, int frames) {
        bank = waveBank;
        tableSize = size;
        numFrames = frames;
    }

    void setFreq(float freq, float sampleRate = 48000.0f) {
        phaseInc = (freq * tableSize) / sampleRate;
    }

    void resetPhase() {
        phase = 0.0f;
    }

    float next(float morphPos) {
        phaseWrapped = false;

        phase += phaseInc;
        if (phase >= tableSize) {
            phase -= tableSize;
            phaseWrapped = true; 
        }

        int indexA = (int)phase;
        int indexB = (indexA + 1) % tableSize;
        float fracX = phase - indexA;

        int frameA = (int)morphPos;
        int frameB = frameA + 1;
        if (frameB >= numFrames) frameB = numFrames - 1; 
        float fracY = morphPos - frameA;

        // Scaling 16-bit array data to floats
        float p00 = bank[(frameA * tableSize) + indexA] / 32768.0f;
        float p10 = bank[(frameA * tableSize) + indexB] / 32768.0f;
        float p01 = bank[(frameB * tableSize) + indexA] / 32768.0f;
        float p11 = bank[(frameB * tableSize) + indexB] / 32768.0f;

        float sampleA = p00 + fracX * (p10 - p00);
        float sampleB = p01 + fracX * (p11 - p01);
        
        return sampleA + fracY * (sampleB - sampleA);
    }
};