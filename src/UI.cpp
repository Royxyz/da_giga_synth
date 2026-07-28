#include "UI.h"
#include "GlobalState.h"
#include <Arduino.h>
#include <Bounce2.h>

const int POT_PINS[7] = {7, 8, 9, 10, 11, 12, 13};

#define ENCODER_PIN_A 14
#define ENCODER_PIN_B 15
#define ENCODER_SW 16

#define BTN_1 17
#define BTN_2 18
#define BTN_3 21
#define BTN_4 38

Bounce2::Button btn1 = Bounce2::Button();
Bounce2::Button btn2 = Bounce2::Button();
Bounce2::Button btn3 = Bounce2::Button();
Bounce2::Button btn4 = Bounce2::Button();
Bounce2::Button encSw = Bounce2::Button();

// Smoothing variables to prevent crackling from knob jumps
float sRoot = 50.0f;
float sSpread = 0.0f;

int lastEncoderState;
int octaveShift = 0;

void initUI() {
    for (int i = 0; i < 7; i++) pinMode(POT_PINS[i], INPUT);
    
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);

    btn1.attach(BTN_1, INPUT_PULLUP);
    btn2.attach(BTN_2, INPUT_PULLUP);
    btn3.attach(BTN_3, INPUT_PULLUP);
    btn4.attach(BTN_4, INPUT_PULLUP);
    encSw.attach(ENCODER_SW, INPUT_PULLUP);

    btn1.interval(15); btn2.interval(15); btn3.interval(15); btn4.interval(15); encSw.interval(15);
    btn1.setPressedState(LOW); btn2.setPressedState(LOW); btn3.setPressedState(LOW); btn4.setPressedState(LOW); encSw.setPressedState(LOW);

    lastEncoderState = digitalRead(ENCODER_PIN_A);
}

void uiTask(void *pvParameters) {
    initUI();

    while (true) {
        btn1.update(); btn2.update(); btn3.update(); btn4.update(); encSw.update();

        // Toggle mutes on button press
        if(btn1.pressed()) state.mute1 = !state.mute1;
        if(btn2.pressed()) state.mute2 = !state.mute2;
        if(btn3.pressed()) state.mute3 = !state.mute3;
        if(btn4.pressed()) state.mute4 = !state.mute4;
        
        if(encSw.pressed()) octaveShift = 0; // Reset octave

        // Read Encoder
        int encA = digitalRead(ENCODER_PIN_A);
        if (encA != lastEncoderState && encA == LOW) {
            if (digitalRead(ENCODER_PIN_B) == HIGH) octaveShift++;
            else octaveShift--;
            octaveShift = constrain(octaveShift, -2, 2);
        }
        lastEncoderState = encA;

        // Read Pots
        float targetRoot = map(analogRead(POT_PINS[0]), 0, 4095, 30, 300);
        float targetSpread = analogRead(POT_PINS[1]) / 4095.0f;
        
        // Low-pass filter the pots for continuous, crackle-free gliding
        sRoot += 0.05f * (targetRoot - sRoot);
        sSpread += 0.05f * (targetSpread - sSpread);

        state.detune = map(analogRead(POT_PINS[2]), 0, 4095, 0, 15); 
        state.fmAmount = map(analogRead(POT_PINS[3]), 0, 4095, 0, 800);
        state.filterCutoff = map(analogRead(POT_PINS[4]), 0, 4095, 10, 255);
        state.lfoRate = map(analogRead(POT_PINS[5]), 0, 4095, 1, 80) / 10.0f;
        
        state.delayWash = map(analogRead(POT_PINS[6]), 0, 4095, 0, 220); // Max 220 to prevent blown-out feedback

        // Calculate Drone Intervals based on spread and octave
        float base = sRoot * pow(2, octaveShift);
        state.freq1 = (int)base;
        state.freq2 = (int)(base + (sSpread * base * 0.5f)); // Morphs up to a Fifth
        state.freq3 = (int)(base + (sSpread * base));        // Morphs up to an Octave
        state.freq4 = (int)(base + (sSpread * base * 1.5f)); 

        vTaskDelay(pdMS_TO_TICKS(15));
    }
}