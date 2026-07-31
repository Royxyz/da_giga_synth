#pragma once
#include <Arduino.h>

class FloatEnvelope {
public:
    enum EnvState { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

private:
    EnvState state = IDLE;
    float output = 0.0f;
    float attackInc, decayInc, releaseInc;
    float sustainLevel;
    float sampleRate;

public:
    FloatEnvelope(float sr = 48000.0f) : sampleRate(sr) {
        setADSR(10.0f, 100.0f, 0.5f, 200.0f); 
    }

    void setADSR(float a_ms, float d_ms, float s_lvl, float r_ms) {

        if (a_ms < 1.0f) a_ms = 1.0f;
        if (d_ms < 1.0f) d_ms = 1.0f;
        if (r_ms < 1.0f) r_ms = 1.0f;

        attackInc = 1.0f / (a_ms * 0.001f * sampleRate);
        decayInc = 1.0f / (d_ms * 0.001f * sampleRate);
        releaseInc = 1.0f / (r_ms * 0.001f * sampleRate);
        sustainLevel = s_lvl;
    }

    void noteOn() {
        state = ATTACK;
    }

    void noteOff() {
        if (state != IDLE) state = RELEASE;
    }

    float process() {
        switch (state) {
            case IDLE:
                output = 0.0f;
                break;
            case ATTACK:
                output += attackInc;
                if (output >= 1.0f) { 
                    output = 1.0f; 
                    state = DECAY; 
                }
                break;
            case DECAY:
                output -= decayInc;
                if (output <= sustainLevel) { 
                    output = sustainLevel; 
                    state = SUSTAIN; 
                }
                break;
            case SUSTAIN:
                output = sustainLevel;
                break;
            case RELEASE:
                output -= releaseInc;
                if (output <= 0.0f) { 
                    output = 0.0f; 
                    state = IDLE; 
                }
                break;
        }
        return output;
    }
    
    bool isActive() const { return state != IDLE; }
};