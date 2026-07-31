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
    FloatWavetable osc2;
    FloatEnvelope ampEnv;
    int currentNote = -1;

    Voice(const int16_t* b1, const int16_t* b2, int size, int frames)
        : osc1(b1, size, frames), osc2(b2, size, frames) {}
};

class VoiceManager {
private:
    Voice* voices[MAX_VOICES];
    int tableSize;
    int numFrames;
    bool hardSync = false; 

public:
    VoiceManager(const int16_t* bank1, const int16_t* bank2, int size, int frames) {
        tableSize = size;
        numFrames = frames;
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i] = new Voice(bank1, bank2, size, frames);
        }
    }

    void setBanks(const int16_t* bank1, const int16_t* bank2) {
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i]->osc1.setTable(bank1, tableSize, numFrames);
            voices[i]->osc2.setTable(bank2, tableSize, numFrames);
        }
    }

    void setSync(bool syncEnabled) {
        hardSync = syncEnabled;
    }

    void setEnvelopes(float a, float d, float s, float r) {
        for(int i = 0; i < MAX_VOICES; i++) {
            voices[i]->ampEnv.setADSR(a, d, s, r);
        }
    }

    void noteOn(uint8_t note, float detuneSemi, int octDrop) {
        int voiceIdx = -1;
        for(int i = 0; i < MAX_VOICES; i++) {
            if(!voices[i]->ampEnv.isActive()) { voiceIdx = i; break; }
        }
        if(voiceIdx == -1) voiceIdx = 0; 

        float freq1 = fast_mtof((float)note);
        float osc2Pitch = (float)note - (float)(octDrop * 12) + detuneSemi;
        float freq2 = fast_mtof(osc2Pitch);

        voices[voiceIdx]->currentNote = note;
        voices[voiceIdx]->osc1.setFreq(freq1);
        voices[voiceIdx]->osc2.setFreq(freq2);
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

    float process(float morph1, float morph2) {
        float rawSum = 0.0f;
        
        for(int i = 0; i < MAX_VOICES; i++) {
            if(voices[i]->ampEnv.isActive()) {

                float raw1 = voices[i]->osc1.next(morph1);

                if (hardSync && voices[i]->osc1.phaseWrapped) {
                    voices[i]->osc2.resetPhase();
                }
                
                float raw2 = voices[i]->osc2.next(morph2);
                float env = voices[i]->ampEnv.process();
                
                rawSum += ((raw1 + raw2) * 0.5f) * env;
            }
        }
        
        return tanhf(rawSum); 
    }
};