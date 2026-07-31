#pragma once
#include "IMidiHandler.h"

class DummySequencer : public IMidiHandler {
private:
    unsigned long lastStepTime = 0;
    int currentStep = 0;
    
    // Slowed down to 1200ms per step to let those lush chords and reverbs breathe
    int stepDuration = 1200; 

    // 16 Measures transcribed from the MusicXML.
    // Each measure contains 4 simultaneous MIDI notes (Bass, Chord, Melody)
    uint8_t pattern[16][4] = {
        {57, 67, 72, 74}, // M1:  A3, G4, C5, D5  (Am11)
        {57, 67, 72, 76}, // M2:  A3, G4, C5, E5
        {57, 67, 72, 74}, // M3:  A3, G4, C5, D5
        {57, 67, 72, 76}, // M4:  A3, G4, C5, E5
        
        {48, 67, 71, 74}, // M5:  C3, G4, B4, D5  (Cmaj9)
        {48, 67, 71, 79}, // M6:  C3, G4, B4, G5
        {48, 67, 71, 74}, // M7:  C3, G4, B4, D5
        {48, 67, 71, 83}, // M8:  C3, G4, B4, B5
        
        {52, 62, 67, 69}, // M9:  E3, D4, G4, A4  (Em11)
        {52, 62, 67, 71}, // M10: E3, D4, G4, B4
        {52, 62, 67, 66}, // M11: E3, D4, G4, F#4
        {52, 62, 66, 76}, // M12: E3, D4, F#4, E5
        
        {50, 57, 66, 76}, // M13: D3, A3, F#4, E5 (D6/9)
        {50, 57, 66, 71}, // M14: D3, A3, F#4, B4
        {50, 57, 71, 67}, // M15: D3, A3, B4, G4
        {50, 57, 66, 74}  // M16: D3, A3, F#4, D5
    };
    
    // Track currently playing notes to send noteOffs
    uint8_t activeNotes[4] = {0, 0, 0, 0};

public:
    void begin() override {
        lastStepTime = millis();
    }

    void update() override {
        unsigned long now = millis();
        if (now - lastStepTime >= stepDuration) {
            lastStepTime = now;

            // 1. Send NoteOff for all currently active notes in the previous chord
            for (int i = 0; i < 4; i++) {
                if (activeNotes[i] != 0 && onNoteOff) {
                    onNoteOff(activeNotes[i], 0);
                    activeNotes[i] = 0;
                }
            }

            // 2. Send NoteOn for the new 4-note chord
            for (int i = 0; i < 4; i++) {
                uint8_t note = pattern[currentStep][i];
                if (note != 0 && onNoteOn) {
                    onNoteOn(note, 100); // Standard velocity
                    activeNotes[i] = note;
                }
            }
            
            // 3. Advance the sequencer
            currentStep = (currentStep + 1) % 16;
        }
    }
};