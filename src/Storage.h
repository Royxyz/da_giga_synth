#pragma once
#include <Arduino.h>

bool initStorage();

bool loadWavetableToBank(const char* filename, int targetBank);

extern int16_t* activeBank1;
extern int16_t* activeBank2;
extern int16_t* activeBank3;
extern int16_t* loadBuffer;