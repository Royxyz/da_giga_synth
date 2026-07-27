#pragma once
#include "NoteInput.h"

class PatternSequencer : public INoteSource {
public:
    PatternSequencer(uint32_t bpm);
    
    void update() override;
    bool hasEvent() override;
    NoteEvent popEvent() override;

private:
    uint32_t stepIntervalMs;
    uint32_t lastStepTime;
    uint8_t currentStep;
    NoteEvent pendingEvent;
    
    // A single, deep C1 note
    const uint8_t pattern[1] = {36}; 
};