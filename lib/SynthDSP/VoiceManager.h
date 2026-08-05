#pragma once
#include <Arduino.h>
#include "FloatWavetable.h"
#include "FloatEnvelope.h"
#include "FloatLFO.h"
#include "FloatSVF.h"
#include "GlobalState.h"

#define MAX_VOICES 6 

inline float fast_mtof(float midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69.0f) / 12.0f);
}

struct Voice {
    FloatWavetable osc1;
    FloatWavetable osc2;
    FloatEnvelope ampEnv;
    FloatEnvelope modEnv1;
    FloatEnvelope modEnv2;
    FloatLFO lfo2;    
    FloatSVF filter;  

    int currentNote = -1;
    float velocity = 0.0f;
    uint32_t noteOnTime = 0; 
    
    // --- NEW: Cached targets to prevent CPU starvation ---
    float targetM1 = 0.0f;
    float targetM2 = 0.0f;
    float targetAmp = 0.0f;

    Voice(const int16_t* b1, const int16_t* b2, int size, int frames, float sr)
        : osc1(b1, size, frames), osc2(b2, size, frames), 
          ampEnv(sr), modEnv1(sr), modEnv2(sr), lfo2(sr) {}
};

class VoiceManager {
private:
    Voice* voices[MAX_VOICES];
    int tableSize;
    int numFrames;
    float sampleRate; 
    uint32_t timeCounter = 0; 
    
    // --- NEW: Control Rate Divider ---
    int localControlCounter = 0; 

public:
    VoiceManager(const int16_t* bank1, const int16_t* bank2, int size, int frames, float sr = 48000.0f) {
        tableSize = size;
        numFrames = frames;
        sampleRate = sr;
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i] = new Voice(bank1, bank2, size, frames, sr);
        }
    }

    void setBanks(const int16_t* bank1, const int16_t* bank2) {
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i]->osc1.setTable(bank1, tableSize, numFrames);
            voices[i]->osc2.setTable(bank2, tableSize, numFrames);
        }
    }

    void setEnvelopes(float aA, float aD, float aS, float aR,
                      float m1A, float m1D, float m1S, float m1R,
                      float m2A, float m2D, float m2S, float m2R) {
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i]->ampEnv.setADSR(aA, aD, aS, aR);
            voices[i]->modEnv1.setADSR(m1A, m1D, m1S, m1R);
            voices[i]->modEnv2.setADSR(m2A, m2D, m2S, m2R);
        }
    }

    void noteOn(uint8_t note, uint8_t vel) {
        timeCounter++;
        int voiceIdx = -1;

        for(int i = 0; i < MAX_VOICES; i++) {
            if(voices[i]->currentNote == note) { voiceIdx = i; break; }
        }

        if(voiceIdx == -1) {
            for(int i = 0; i < MAX_VOICES; i++) {
                if(!voices[i]->ampEnv.isActive()) { voiceIdx = i; break; }
            }
        }

        if(voiceIdx == -1) {
            uint32_t oldestTime = 0xFFFFFFFF;
            for(int i = 0; i < MAX_VOICES; i++) {
                if(voices[i]->noteOnTime < oldestTime) {
                    oldestTime = voices[i]->noteOnTime;
                    voiceIdx = i;
                }
            }
        }

        voices[voiceIdx]->currentNote = note;
        voices[voiceIdx]->velocity = (float)vel / 127.0f;
        voices[voiceIdx]->noteOnTime = timeCounter;

        voices[voiceIdx]->filter.reset();
        voices[voiceIdx]->ampEnv.reset();
        voices[voiceIdx]->modEnv1.reset();
        voices[voiceIdx]->modEnv2.reset();
        voices[voiceIdx]->osc1.resetPhase();
        voices[voiceIdx]->osc2.resetPhase();
        
        voices[voiceIdx]->ampEnv.noteOn();
        voices[voiceIdx]->modEnv1.noteOn();
        voices[voiceIdx]->modEnv2.noteOn();
        voices[voiceIdx]->lfo2.resetPhase();
    }

    void noteOff(uint8_t note) {
        for(int i = 0; i < MAX_VOICES; i++) {
            if(voices[i]->currentNote == note) {
                voices[i]->ampEnv.noteOff();
                voices[i]->modEnv1.noteOff();
                voices[i]->modEnv2.noteOff();
            }
        }
    }

    float process(float globalLFO1) {
        float rawSum = 0.0f;
        
        // --- Execute heavy math only every 46 samples (~1ms) ---
        bool doControl = false;
        if (++localControlCounter >= 46) {
            doControl = true;
            localControlCounter = 0;
        }

        float baseMorph1 = state.osc1BaseMorph.load();
        float baseMorph2 = state.osc2BaseMorph.load();
        float baseCutoff = state.filterCutoff.load();
        float filterRes  = state.filterRes.load();
        int   filtMode   = state.filterMode.load();
        float oscMix     = state.oscMix.load();
        int   lfo2Wave   = state.lfo2Wave.load();
        
        // Get Coarse Tuning
        float c1Tune = state.osc1Coarse.load();
        float c2Tune = state.osc2Coarse.load();

        for(int i = 0; i < MAX_VOICES; i++) {
            if(voices[i]->ampEnv.isActive()) {
                Voice* v = voices[i];

                // 1. Audio-Rate Envelopes (Fast Math)
                float envAmp = v->ampEnv.process();
                float mod1 = v->modEnv1.process();
                float mod2 = v->modEnv2.process();
                float lfo2 = v->lfo2.process(lfo2Wave);

                // 2. Control-Rate Matrix & Filter Update (Heavy Math)
                if (doControl) {
                    float src[6] = { v->velocity, envAmp, mod1, mod2, globalLFO1, lfo2 };
                    float dest[5] = {0.0f}; 
                    
                    for(int s = 0; s < 6; s++) {
                        for(int d = 0; d < 5; d++) {
                            dest[d] += src[s] * state.modMatrix[(s * 5) + d].load();
                        }
                    }

                    // Pitch Math
                    float pitchMod = powf(2.0f, dest[1]); 
                    float baseFreq = fast_mtof((float)v->currentNote);
                    v->osc1.setFreq(baseFreq * pitchMod * powf(2.0f, c1Tune / 12.0f), sampleRate);
                    v->osc2.setFreq(baseFreq * pitchMod * powf(2.0f, c2Tune / 12.0f), sampleRate);

                    // Morph Targets
                    v->targetM1 = baseMorph1 + (dest[2] * 128.0f);
                    v->targetM2 = baseMorph2 + (dest[3] * 128.0f);
                    if (v->targetM1 < 0.0f) v->targetM1 = 0.0f; if (v->targetM1 > 127.9f) v->targetM1 = 127.9f;
                    if (v->targetM2 < 0.0f) v->targetM2 = 0.0f; if (v->targetM2 > 127.9f) v->targetM2 = 127.9f;

                    // Filter Math
                    float currentCutoff = baseCutoff * powf(2.0f, dest[4] * 4.0f); 
                    if (currentCutoff < 20.0f) currentCutoff = 20.0f;
                    if (currentCutoff > 20000.0f) currentCutoff = 20000.0f;
                    v->filter.setCutoffRes(currentCutoff, filterRes, sampleRate);
                    
                    v->targetAmp = dest[0];
                }

                // 3. Audio-Rate Generation
                float raw1 = v->osc1.next(v->targetM1);
                float raw2 = v->osc2.next(v->targetM2);
                float blendedOsc = (raw1 * (1.0f - oscMix)) + (raw2 * oscMix);
                
                v->filter.process(blendedOsc);
                
                float filteredSound = v->filter.lp;
                if (filtMode == 1) filteredSound = v->filter.bp;
                if (filtMode == 2) filteredSound = v->filter.hp;

                float ampTotal = envAmp + v->targetAmp; 
                if (ampTotal < 0.0f) ampTotal = 0.0f;
                
                rawSum += filteredSound * ampTotal;

            } else {
                voices[i]->currentNote = -1; 
            }
        }
        
        // --- NEW: Add Headroom (* 0.25f) before the Master Saturation ---
        return tanhf(rawSum * 0.25f); 
    }
};