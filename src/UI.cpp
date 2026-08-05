#include "UI.h"
#include "GlobalState.h"
#include <Bounce2.h>

// --- Mux Pins ---
const int MUX_Z  = 7;  
const int MUX_S0 = 8;  
const int MUX_S1 = 9;  
const int MUX_S2 = 46; 

// --- Retained Button Pins ---
const int BTN_OSC1_BANK = 39; 
const int BTN_OSC2_BANK = 17; // Repurposed
const int BTN_FILT_MODE = 18; // Repurposed
const int BTN_FX_FREEZE = 21;
const int BTN_LFO1_WAVE = 45; // Repurposed
const int BTN_LFO2_WAVE = 47; // Repurposed
const int BTN_LFO1_RST  = 48; // Repurposed

// --- Encoder ---
const int ENC_A = 14;
const int ENC_B = 15;
const int ENC_SW = 16;

const int NUM_BUTTONS = 7;
Bounce2::Button buttons[NUM_BUTTONS];
const int BUTTON_PINS[NUM_BUTTONS] = {
    BTN_OSC1_BANK, BTN_OSC2_BANK, BTN_FILT_MODE, BTN_FX_FREEZE,
    BTN_LFO1_WAVE, BTN_LFO2_WAVE, BTN_LFO1_RST
};

Bounce2::Button btnEnc = Bounce2::Button();

volatile int encoderPos = 0;
volatile int lastEncoded = 0;

void IRAM_ATTR encoderISR() {
    int MSB = digitalRead(ENC_A);
    int LSB = digitalRead(ENC_B);
    int encoded = (MSB << 1) | LSB;
    int sum  = (lastEncoded << 2) | encoded;

    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {encoderPos = encoderPos + 1;}
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {encoderPos = encoderPos - 1;}
    
    lastEncoded = encoded;
}

int readMux(int channel) {
    digitalWrite(MUX_S0, bitRead(channel, 0));
    digitalWrite(MUX_S1, bitRead(channel, 1));
    digitalWrite(MUX_S2, bitRead(channel, 2));
    delayMicroseconds(10); 
    return analogRead(MUX_Z);
}

void initUI() {
    pinMode(MUX_Z, INPUT);
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
        buttons[i].attach(BUTTON_PINS[i], INPUT_PULLUP);
        buttons[i].interval(5);
        buttons[i].setPressedState(LOW);
    }

    btnEnc.attach(ENC_SW, INPUT_PULLUP);
    btnEnc.interval(5);
    btnEnc.setPressedState(LOW);

    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), encoderISR, CHANGE);
}

float mapfLog(float x, float in_min, float in_max, float out_min, float out_max) {
    if (x <= in_min) return out_min;
    if (x >= in_max) return out_max;
    float log_min = log(out_min);
    float log_max = log(out_max);
    float log_val = log_min + (x - in_min) * (log_max - log_min) / (in_max - in_min);
    return exp(log_val);
}

void uiTask(void *pvParameters) {
    initUI();
    int lastEncPos = 0;

    // --- EMA Jitter Smoothing Array ---
    float emaPots[7] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const float emaAlpha = 0.15f; // Lower = smoother but slower response

    for(;;) {
        // --- 1. Read & Smooth Pots ---
        for (int i = 0; i < 7; i++) {
            float rawVal = (float)readMux(i);
            emaPots[i] = (emaPots[i] * (1.0f - emaAlpha)) + (rawVal * emaAlpha);
        }

        // --- 2. Map Smoothed Values to Hardware Macros ---
        
        // Pot 0: Filter Cutoff (Logarithmic)
        // Squeezed the max ADC value down to 3100 to guarantee it hits 20kHz at 2.6V
        state.filterCutoff.store(mapfLog(emaPots[0], 150.0f, 3100.0f, 20.0f, 20000.0f));    
        
        // Pot 1: Filter Resonance (Linear 0.0 to 0.95)
        state.filterRes.store(emaPots[1] * (0.95f / 4095.0f));
        
        // Pot 2: Osc 1 Base Morph (Linear 0.0 to 127.9 for 128 frames)
        state.osc1BaseMorph.store(emaPots[2] * (127.9f / 3900.0f));
        
        // Pot 3: Osc 2 Base Morph (Linear 0.0 to 127.9 for 128 frames)
        state.osc2BaseMorph.store(emaPots[3] * (127.9f / 4095.0f));
        
        // Pot 4: LFO 1 Rate (Global Metronome, Logarithmic 0.1Hz to 40.0Hz)
        state.lfo1Rate.store(mapfLog(emaPots[4], 0.0f, 4095.0f, 0.1f, 40.0f));
        
        // Pot 5: Abyss Reverb Send (Linear 0.0 to 1.0)
        state.abyssSend.store(emaPots[5] / 4095.0f);
        
        // Pot 6: Oscillator Mix (Linear 0.0 to 1.0)
        state.oscMix.store(emaPots[6] / 4095.0f);

        // --- 3. Update Buttons ---
        for (int i = 0; i < NUM_BUTTONS; i++) buttons[i].update();
        btnEnc.update();

        if (buttons[0].pressed()) state.osc1Bank.store((state.osc1Bank.load() + 1) % 3);
        if (buttons[1].pressed()) state.osc2Bank.store((state.osc2Bank.load() + 1) % 3);
        if (buttons[2].pressed()) state.filterMode.store((state.filterMode.load() + 1) % 3);

        if (buttons[3].pressed()) state.fxFreeze.store(!state.fxFreeze.load()); 
        
        if (buttons[4].pressed()) state.lfo1Wave.store((state.lfo1Wave.load() + 1) % 4);
        if (buttons[5].pressed()) state.lfo2Wave.store((state.lfo2Wave.load() + 1) % 4);
  
        if (buttons[6].pressed()) state.fxFreeze.store(false); 

        if (btnEnc.pressed()) state.fxMode.store((state.fxMode.load() + 1) % 2); // Toggle Abyss Mode
        
        if (encoderPos != lastEncPos) {
            float newRate = state.lfo2Rate.load() + (encoderPos > lastEncPos ? 0.2f : -0.2f);
            if (newRate < 0.1f) newRate = 0.1f;
            if (newRate > 40.0f) newRate = 40.0f;
            state.lfo2Rate.store(newRate);
            lastEncPos = encoderPos;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}