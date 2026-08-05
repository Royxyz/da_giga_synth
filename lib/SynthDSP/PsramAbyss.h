#pragma once
#include <Arduino.h>

#define ABYSS_SIZE 262144 

class PsramAbyss {
private:
    float* cloud;
    uint32_t writeHead = 0;

    // Triple LFOs for deep smearing
    float lfoPhase1 = 0.0f;
    float lfoPhase2 = 0.0f;
    float lfoPhase3 = 0.0f;

    // Darkens the reverb tails
    float lpFeedback = 0.0f; 

public:
    bool init() {
        cloud = (float*)ps_malloc(ABYSS_SIZE * sizeof(float));
        if (cloud == NULL) {
            return false; 
        }
        memset(cloud, 0, ABYSS_SIZE * sizeof(float));
        return true;
    }

    float process(float input, int mode, bool freeze) {
        if (cloud == NULL) return 0.0f; 

        // 1. Advance the three chorus LFOs
        lfoPhase1 += 0.00013f;
        lfoPhase2 += 0.00017f;
        lfoPhase3 += 0.00009f;
        if (lfoPhase1 > TWO_PI) lfoPhase1 -= TWO_PI;
        if (lfoPhase2 > TWO_PI) lfoPhase2 -= TWO_PI;
        if (lfoPhase3 > TWO_PI) lfoPhase3 -= TWO_PI;

        // Modulate tap positions for diffusion
        float mod1 = sinf(lfoPhase1) * 45.0f;
        float mod2 = sinf(lfoPhase2) * 60.0f;
        float mod3 = sinf(lfoPhase3) * 85.0f;

        // 2. Diffused Taps (Spaced to avoid metallic ringing)
        int tap1 = (writeHead - 13107 + (int)mod1 + ABYSS_SIZE) % ABYSS_SIZE;
        int tap2 = (writeHead - 31793 + (int)mod2 + ABYSS_SIZE) % ABYSS_SIZE;
        int tap3 = (writeHead - 76541 + (int)mod3 + ABYSS_SIZE) % ABYSS_SIZE;
        int tap4 = (writeHead - 153083 + ABYSS_SIZE) % ABYSS_SIZE; // Deep tail

        float read1 = cloud[tap1];
        float read2 = cloud[tap2];
        float read3 = cloud[tap3];
        float read4 = cloud[tap4];

        // 3. Mixing and Feedback Generation
        float reverbOut = 0.0f;
        float feedback = 0.0f;

        if (mode == 0) {
            // Mode 0: Shorter, denser room/chorus
            reverbOut = (read1 + read2 + read3) * 0.33f; 
            feedback = reverbOut * 0.70f;
        } else {
            // Mode 1: Massive ambient cloud
            reverbOut = (read1 + read2 + read3 + read4) * 0.25f;
            feedback = reverbOut * 0.88f; // Heavy feedback
        }

        // 4. Dampen the reverb tails (1-pole lowpass)
        lpFeedback = (lpFeedback * 0.4f) + (feedback * 0.6f);

        float writeSample = 0.0f;
        if (freeze) {
            writeSample = reverbOut; 
            reverbOut = writeSample; 
        } else {
            writeSample = input + lpFeedback; 
        }

        // Soft clip the write head to prevent infinite volume explosion
        writeSample = tanhf(writeSample);
        cloud[writeHead] = writeSample;
        writeHead = (writeHead + 1) % ABYSS_SIZE;

        return reverbOut; 
    }
};