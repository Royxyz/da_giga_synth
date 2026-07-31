#pragma once
#include "IMidiHandler.h"

class DummySequencer : public IMidiHandler {
private:
    unsigned long lastStepTime = 0;
    int currentStep = 0;
    int stepDuration = 125; 

    uint8_t pattern[16] = {
        36, 0, 36, 36, 
        39, 0, 36, 0, 
        41, 0, 36, 39, 
        36, 43, 0, 36
    };
    
    uint8_t lastNote = 0;

public:
    void begin() override {
        lastStepTime = millis();
    }

    void update() override {
        unsigned long now = millis();
        if (now - lastStepTime >= stepDuration) {
            lastStepTime = now;

            if (lastNote != 0 && onNoteOff) {
                onNoteOff(lastNote, 0);
                lastNote = 0;
            }

            uint8_t note = pattern[currentStep];
            if (note != 0 && onNoteOn) {
                onNoteOn(note, 100); 
                lastNote = note;
            }
            
            currentStep = (currentStep + 1) % 16;
        }
    }
};