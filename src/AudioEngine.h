#pragma once
#include <Arduino.h>

void audioTask(void *pvParameters);

void engineNoteOn(uint8_t note, uint8_t velocity);
void engineNoteOff(uint8_t note, uint8_t velocity);