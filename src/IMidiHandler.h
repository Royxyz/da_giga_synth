#pragma once
#include <Arduino.h>
#include <functional>

using NoteOnCallback = std::function<void(uint8_t note, uint8_t velocity)>;
using NoteOffCallback = std::function<void(uint8_t note, uint8_t velocity)>;

class IMidiHandler {
protected:
    NoteOnCallback onNoteOn;
    NoteOffCallback onNoteOff;

public:
    virtual ~IMidiHandler() {}
    
    virtual void begin() = 0;
    virtual void update() = 0;
    
    void setCallbacks(NoteOnCallback onCb, NoteOffCallback offCb) {
        onNoteOn = onCb;
        onNoteOff = offCb;
    }
};