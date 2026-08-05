#pragma once
#include "IMidiHandler.h"

class DummySequencer : public IMidiHandler {
private:
    unsigned long lastStepTime = 0;
    int currentStep = 0;
    
    // 125ms = 16th note at 120 BPM
    int stepDuration = 125; 

    // Two lush 4-note chords: Fm9 and Ebmaj9
    uint8_t chords[2][4] = {
        {53, 60, 63, 67}, // Fm9    (F3, C4, Eb4, G4)
        {51, 58, 62, 65}  // Ebmaj9 (Eb3, Bb3, D4, F4)
    };

    // The classic 3-3-2 dotted 8th rhythm 
    // (1 = Trigger Gate, 0 = Rest)
    uint8_t rhythm[16] = {
        1, 0, 0, 1,   0, 0, 1, 0, 
        1, 0, 0, 1,   0, 0, 1, 0
    };

    // Which chord block to play (0 for Fm9, 1 for Ebmaj9)
    uint8_t chordProgression[16] = {
        0, 0, 0, 0,   0, 0, 0, 0,
        1, 1, 1, 1,   1, 1, 1, 1
    };
    
    uint8_t activeNotes[4] = {0, 0, 0, 0};

public:
    void begin() override {
        lastStepTime = millis();
    }

    void update() override {
        unsigned long now = millis();
        if (now - lastStepTime >= stepDuration) {
            lastStepTime = now;

            // 1. Cut the previous chord exactly on the grid to create the stab
            for (int i = 0; i < 4; i++) {
                if (activeNotes[i] != 0 && onNoteOff) {
                    onNoteOff(activeNotes[i], 0);
                    activeNotes[i] = 0;
                }
            }

            // 2. If this step is a rhythmic strike, fire the whole chord simultaneously
            if (rhythm[currentStep]) {
                int chordIdx = chordProgression[currentStep];
                for (int i = 0; i < 4; i++) {
                    uint8_t note = chords[chordIdx][i];
                    if (note != 0 && onNoteOn) {
                        onNoteOn(note, 100); 
                        activeNotes[i] = note;
                    }
                }
            }
            
            currentStep = (currentStep + 1) % 16;
        }
    }
};