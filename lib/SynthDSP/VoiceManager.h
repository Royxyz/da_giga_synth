#pragma once
#include <Arduino.h>
#include "FloatWavetable.h"
#include "FloatEnvelope.h"

#define MAX_VOICES 6 

inline float fast_mtof(float midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69.0f) / 12.0f);
}

struct Voice {
    FloatWavetable osc1;
    FloatEnvelope ampEnv;
    int currentNote = -1;
    uint32_t noteOnTime = 0; // Tracks the age of the note for smart stealing

    Voice(const int16_t* b1, int size, int frames)
        : osc1(b1, size, frames) {}
};

class VoiceManager {
private:
    Voice* voices[MAX_VOICES];
    int tableSize;
    int numFrames;
    float sampleRate; 
    uint32_t timeCounter = 0; // Global counter to track voice age

public:
    VoiceManager(const int16_t* bank1, int size, int frames, float sr = 48000.0f) {
        tableSize = size;
        numFrames = frames;
        sampleRate = sr;
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i] = new Voice(bank1, size, frames);
        }
    }

    void setBank(const int16_t* bank1) {
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i]->osc1.setTable(bank1, tableSize, numFrames);
        }
    }

    void setEnvelopes(float a, float d, float s, float r) {
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i]->ampEnv.setADSR(a, d, s, r);
        }
    }

    void noteOn(uint8_t note) {
        timeCounter++;
        int voiceIdx = -1;

        // 1. Check if this exact note is already playing (prevent phase-buildup)
        for(int i = 0; i < MAX_VOICES; i++) {
            if(voices[i]->currentNote == note) { 
                voiceIdx = i; 
                break; 
            }
        }

        // 2. If not, find a completely silent voice
        if(voiceIdx == -1) {
            for(int i = 0; i < MAX_VOICES; i++) {
                if(!voices[i]->ampEnv.isActive()) { 
                    voiceIdx = i; 
                    break; 
                }
            }
        }

        // 3. If all voices are busy, steal the oldest one
        if(voiceIdx == -1) {
            uint32_t oldestTime = 0xFFFFFFFF;
            for(int i = 0; i < MAX_VOICES; i++) {
                if(voices[i]->noteOnTime < oldestTime) {
                    oldestTime = voices[i]->noteOnTime;
                    voiceIdx = i;
                }
            }
        }

        // Lock in the note and trigger
        float freq1 = fast_mtof((float)note);
        voices[voiceIdx]->currentNote = note;
        voices[voiceIdx]->noteOnTime = timeCounter;
        voices[voiceIdx]->osc1.setFreq(freq1, sampleRate);
        voices[voiceIdx]->ampEnv.noteOn();
    }

    void noteOff(uint8_t note) {
        for(int i = 0; i < MAX_VOICES; i++) {
            if(voices[i]->currentNote == note) {
                voices[i]->ampEnv.noteOff();
                // We do NOT clear currentNote here. 
                // We let it fade out naturally so we can re-trigger it if needed.
            }
        }
    }

    float process(float morph1) {
        float rawSum = 0.0f;
        
        for(int i = 0; i < MAX_VOICES; i++) {
            if(voices[i]->ampEnv.isActive()) {
                float raw1 = voices[i]->osc1.next(morph1);
                float env = voices[i]->ampEnv.process();
                rawSum += raw1 * env;
            } else {
                // Envelope has completely finished fading out
                voices[i]->currentNote = -1; 
            }
        }
        
        return tanhf(rawSum); 
    }
};