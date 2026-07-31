#include "UI.h"
#include "GlobalState.h"
#include <Bounce2.h>

// --- Mux Pins ---
const int MUX_Z  = 7;  
const int MUX_S0 = 8;  
const int MUX_S1 = 9;  
const int MUX_S2 = 10; 

// --- Retained Button Pins ---
const int BTN_OSC1_BANK = 39; 
const int BTN_FILT_MODE = 17;
const int BTN_FX_MODE   = 18;
const int BTN_FX_FREEZE = 21;
const int BTN_MOD_ENV   = 45;
const int BTN_LFO_WAVE  = 47;
const int BTN_MOD_TARG  = 48;

// --- Encoder ---
const int ENC_A = 14;
const int ENC_B = 15;
const int ENC_SW = 16;

const int NUM_BUTTONS = 7;
Bounce2::Button buttons[NUM_BUTTONS];
const int BUTTON_PINS[NUM_BUTTONS] = {
    BTN_OSC1_BANK, BTN_FILT_MODE, BTN_FX_MODE, BTN_FX_FREEZE,
    BTN_MOD_ENV, BTN_LFO_WAVE, BTN_MOD_TARG
};

Bounce2::Button btnEnc = Bounce2::Button();

volatile int encoderPos = 0;
volatile int lastEncoded = 0;

void IRAM_ATTR encoderISR() {
    int MSB = digitalRead(ENC_A);
    int LSB = digitalRead(ENC_B);
    int encoded = (MSB << 1) | LSB;
    int sum  = (lastEncoded << 2) | encoded;

    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos++;
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos--;
    
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
    float log_min = log(out_min);
    float log_max = log(out_max);
    float log_val = log_min + (x - in_min) * (log_max - log_min) / (in_max - in_min);
    return exp(log_val);
}

void uiTask(void *pvParameters) {
    initUI();
    int lastEncPos = 0;

    for(;;) {
        // --- 1. Read Pots (Mux Channels 0-6) ---
        state.morph1.store(readMux(0) * (7.0f / 4095.0f));
        
        // Pots 1 & 2: Env Attack & Release (Log curve from 1ms to 2000ms)
        state.envAttack.store(mapfLog(readMux(1), 0.0f, 4095.0f, 1.0f, 2000.0f));
        state.envRelease.store(mapfLog(readMux(2), 0.0f, 4095.0f, 1.0f, 2000.0f));
        
        float rawCutoff = readMux(3);
        state.filterCutoff.store(mapfLog(rawCutoff, 0.0f, 4095.0f, 20.0f, 18000.0f));
        state.filterRes.store(readMux(4) * (0.95f / 4095.0f));
        state.modDepth.store(readMux(5) / 4095.0f);
        state.fxMix.store(readMux(6) / 4095.0f);

        // --- 2. Update Buttons ---
        for (int i = 0; i < NUM_BUTTONS; i++) buttons[i].update();
        btnEnc.update();

        // --- 3. Process Button Presses ---
        if (buttons[0].pressed()) state.osc1Bank.store((state.osc1Bank.load() + 1) % 3);
        if (buttons[1].pressed()) state.filterMode.store((state.filterMode.load() + 1) % 3);
        if (buttons[2].pressed()) state.fxMode.store((state.fxMode.load() + 1) % 3);
        state.fxFreeze.store(buttons[3].isPressed()); 
        
        if (buttons[4].pressed()) state.modEnvShape.store((state.modEnvShape.load() + 1) % 3);
        if (buttons[5].pressed()) state.lfoWave.store((state.lfoWave.load() + 1) % 4);
        if (buttons[6].pressed()) state.modTarget.store((state.modTarget.load() + 1) % 3); // Reduced to 3 targets

        // --- 4. Encoder Logic ---
        if (btnEnc.pressed()) state.useModEnv.store(!state.useModEnv.load());
        
        if (encoderPos != lastEncPos) {
            float newRate = state.lfoRate.load() + (encoderPos > lastEncPos ? 0.5f : -0.5f);
            if (newRate < 0.1f) newRate = 0.1f;
            if (newRate > 40.0f) newRate = 40.0f;
            state.lfoRate.store(newRate);
            lastEncPos = encoderPos;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}