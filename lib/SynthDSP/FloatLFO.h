#pragma once
#include <Arduino.h>

class FloatLFO {
private:
    float phase = 0.0f;
    float phaseInc = 0.0f;
    float sampleRate;
    float currentRandom = 0.0f;

public:
    FloatLFO(float sr = 48000.0f) : sampleRate(sr) {}

    void setRate(float hz) {
        if (hz < 0.01f) hz = 0.01f;
        phaseInc = hz / sampleRate;
    }

    void resetPhase() {
        phase = 0.0f;
    }

    float process(int shape) {
        phase += phaseInc;

        if (phase >= 1.0f) {
            phase -= 1.0f;
            currentRandom = ((float)random(20000) / 10000.0f) - 1.0f;
        }

        switch (shape) {
            case 0: 
                return sinf(phase * TWO_PI);
            case 1: 
                return 1.0f - (phase * 2.0f);
            case 2: 
                return (phase < 0.5f) ? 1.0f : -1.0f;
            case 3: 
                return currentRandom;
            default:
                return 0.0f;
        }
    }
};