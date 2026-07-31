#pragma once
#include <Arduino.h>


#define ABYSS_SIZE 262144 

class PsramAbyss {
private:
    float* cloud;
    uint32_t writeHead = 0;

    float lfoPhase1 = 0.0f;
    float lfoPhase2 = 0.0f;

public:
    bool init() {
        cloud = (float*)ps_malloc(ABYSS_SIZE * sizeof(float));
        if (cloud == NULL) {
            return false; 
        }
        memset(cloud, 0, ABYSS_SIZE * sizeof(float));
        return true;
    }


    float process(float input, float mix, int mode, bool freeze) {
        if (cloud == NULL) return input; 


        lfoPhase1 += 0.0001f;
        lfoPhase2 += 0.00017f;
        if (lfoPhase1 > TWO_PI) lfoPhase1 -= TWO_PI;
        if (lfoPhase2 > TWO_PI) lfoPhase2 -= TWO_PI;

        float mod1 = sinf(lfoPhase1) * 20.0f;
        float mod2 = sinf(lfoPhase2) * 35.0f;

        int tap1 = (writeHead - 17471 + (int)mod1 + ABYSS_SIZE) % ABYSS_SIZE;
        int tap2 = (writeHead - 43913 + (int)mod2 + ABYSS_SIZE) % ABYSS_SIZE;
        int tap3 = (writeHead - 91283 + ABYSS_SIZE) % ABYSS_SIZE;
        int tap4 = (writeHead - 150047 + ABYSS_SIZE) % ABYSS_SIZE;

        float read1 = cloud[tap1];
        float read2 = cloud[tap2];
        float read3 = cloud[tap3];
        float read4 = cloud[tap4];

        float reverbOut = 0.0f;
        float feedback = 0.0f;

        if (mode == 0) {
            reverbOut = read3; 
            feedback = read3 * 0.65f;
        } else {
            reverbOut = (read1 + read2 + read3 + read4) * 0.25f;
            feedback = reverbOut * 0.85f;
        }

        float writeSample = 0.0f;
        if (freeze) {
            writeSample = reverbOut; 
            reverbOut = writeSample; 
        } else {
            writeSample = input + feedback;
        }


        writeSample = tanhf(writeSample);

        cloud[writeHead] = writeSample;
        writeHead = (writeHead + 1) % ABYSS_SIZE;

        // Crossfade dry/wet
        return (input * (1.0f - mix)) + (reverbOut * mix);
    }
};