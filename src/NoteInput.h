#pragma once
#include <Arduino.h>

struct NoteEvent {
    uint8_t note;    
    uint8_t velocity; 
    bool active;     
};

class INoteSource {
public:
    virtual ~INoteSource() = default;

    virtual void update() = 0; 

    virtual bool hasEvent() = 0; 
    
    virtual NoteEvent popEvent() = 0; 
};