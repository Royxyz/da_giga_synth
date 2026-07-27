#include "PatternSequencer.h"

PatternSequencer::PatternSequencer(uint32_t bpm) {
    stepIntervalMs = 1000; // The interval doesn't really matter for a continuous drone
    lastStepTime = 0;
    currentStep = 0;
    pendingEvent.active = false;
}

void PatternSequencer::update() {
    uint32_t now = millis();
    if (now - lastStepTime >= stepIntervalMs) {
        lastStepTime = now;
        
        pendingEvent.note = pattern[0]; // Always play the single note
        pendingEvent.velocity = 100; 
        pendingEvent.active = true;
    }
}

bool PatternSequencer::hasEvent() {
    return pendingEvent.active;
}

NoteEvent PatternSequencer::popEvent() {
    pendingEvent.active = false;
    return pendingEvent;
}