#pragma once
#include <Arduino.h>

class FloatSVF {
private:
    float ic1eq = 0.0f, ic2eq = 0.0f;
    float g = 0.0f, k = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    
public:
    float lp = 0.0f, hp = 0.0f, bp = 0.0f;

        void setCutoffRes(float cutoff, float res, float sampleRate = 48000.0f) {
        if (cutoff > sampleRate / 2.5f) cutoff = sampleRate / 2.5f;
        if (res > 0.99f) res = 0.99f;

        g = tanf(PI * cutoff / sampleRate);
        k = 2.0f - (2.0f * res);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }


    float process(float input) {
        float v3 = input - ic2eq;
        float v1 = a1 * ic1eq + a2 * v3;
        float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        
        lp = v2;
        hp = input - k * v1 - v2;
        bp = v1;
        
        return lp; // Change this to return hp or bp based on a mode variable
    }
};