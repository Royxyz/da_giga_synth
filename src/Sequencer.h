#pragma once
#include "IMidiHandler.h"
#include <atomic>
#include <string.h>

#define MAX_STEPS 64
#define MAX_POLYPHONY 6
#define MAX_CONCURRENT_NOTES 16

struct NoteEvent {
    uint8_t note;
    uint8_t length; 
};

struct SequenceData {
    uint8_t numSteps = 16;
    NoteEvent grid[MAX_STEPS][MAX_POLYPHONY];
};

struct ActiveNote {
    uint8_t note;
    uint8_t remainingSteps;
};

class Sequencer : public IMidiHandler {
private:
    unsigned long lastStepTime = 0;
    int currentStep = 0;

    int stepDuration = 125; 

    SequenceData activeSeq;
    SequenceData nextSeq;
    std::atomic<bool> patternPending{false};

    ActiveNote playingNotes[MAX_CONCURRENT_NOTES]; 

public:
    Sequencer() {
        memset(&activeSeq, 0, sizeof(SequenceData));
        memset(&nextSeq, 0, sizeof(SequenceData));
        for(int i = 0; i < MAX_CONCURRENT_NOTES; i++) playingNotes[i] = {0, 0};
        
        // --- DEFAULT PATTERN: Rolling Bassline (C Minor) ---
        activeSeq.numSteps = 16;
        
        // MIDI 36 = C2, 48 = C3, 39 = Eb2, 43 = G2
        uint8_t bass[16] = {36, 36, 48, 36,  39, 36, 48, 36,  36, 36, 48, 36,  43, 39, 36, 48};
        
        for(int i = 0; i < 16; i++) {
            activeSeq.grid[i][0].note = bass[i];
            activeSeq.grid[i][0].length = 1; // 1-step gate for tight plucks
        }
    }

    void begin() override {
        lastStepTime = millis();
    }

    void loadNextPattern(const SequenceData& newData) {
        nextSeq = newData;
        patternPending.store(true);
    }

    void setBPM(float bpm) {
        if (bpm < 20.0f) bpm = 20.0f;
        if (bpm > 300.0f) bpm = 300.0f;
        // 60,000ms per minute / BPM / 4 (for 16th note steps)
        stepDuration = (int)(15000.0f / bpm); 
    }

    int getCurrentStep() { return currentStep; }

    void update() override {
        unsigned long now = millis();
        if (now - lastStepTime >= stepDuration) {
            lastStepTime = now;

            if (currentStep == 0 && patternPending.load()) {
                activeSeq = nextSeq;
                patternPending.store(false);
            }

            for (int i = 0; i < MAX_CONCURRENT_NOTES; i++) {
                if (playingNotes[i].note != 0) {
                    playingNotes[i].remainingSteps--;
                    
                    if (playingNotes[i].remainingSteps == 0) {
                        if (onNoteOff) onNoteOff(playingNotes[i].note, 0);
                        playingNotes[i].note = 0; 
                    }
                }
            }

            for (int i = 0; i < MAX_POLYPHONY; i++) {
                NoteEvent ev = activeSeq.grid[currentStep][i];
                
                if (ev.note != 0 && ev.length > 0) {
                    int slot = -1;
                    for (int j = 0; j < MAX_CONCURRENT_NOTES; j++) {
                        if (playingNotes[j].note == 0) { 
                            slot = j; 
                            break; 
                        }
                    }
                    
                    if (slot != -1) {
                        if (onNoteOn) onNoteOn(ev.note, 100);
                        playingNotes[slot].note = ev.note;
                        playingNotes[slot].remainingSteps = ev.length;
                    }
                }
            }

            currentStep++;
            if (currentStep >= activeSeq.numSteps) {
                currentStep = 0;
            }
        }
    }
};

extern Sequencer sequencer;