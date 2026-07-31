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

    // Upgraded to int16_t*
    Voice(const int16_t* b1, int size, int frames)
        : osc1(b1, size, frames) {}
};

class VoiceManager {
private:
    Voice* voices[MAX_VOICES];
    int tableSize;
    int numFrames;
    float sampleRate; 

public:
    // Upgraded to int16_t*
    VoiceManager(const int16_t* bank1, int size, int frames, float sr = 48000.0f) {
        tableSize = size;
        numFrames = frames;
        sampleRate = sr;
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i] = new Voice(bank1, size, frames);
        }
    }

    // Upgraded to int16_t*
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
        int voiceIdx = -1;
        for(int i = 0; i < MAX_VOICES; i++) {
            if(!voices[i]->ampEnv.isActive()) { voiceIdx = i; break; }
        }
        if(voiceIdx == -1) voiceIdx = 0; // Simple stealing

        float freq1 = fast_mtof((float)note);
        voices[voiceIdx]->currentNote = note;
        voices[voiceIdx]->osc1.setFreq(freq1, sampleRate);
        voices[voiceIdx]->ampEnv.noteOn();
    }

    void noteOff(uint8_t note) {
        for(int i = 0; i < MAX_VOICES; i++) {
            if(voices[i]->currentNote == note) {
                voices[i]->ampEnv.noteOff();
                voices[i]->currentNote = -1; 
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
            }
        }
        
        return tanhf(rawSum); 
    }
};
