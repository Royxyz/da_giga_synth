#include "UI.h"
#include "GlobalState.h"
#include <Bounce2.h>

// --- NEW: 74HC4051 Mux Pins ---
const int MUX_Z  = 7;  // Common Analog Signal
const int MUX_S0 = 8;  // Select 0
const int MUX_S1 = 9;  // Select 1
const int MUX_S2 = 10; // Select 2

// Group A: Trigger Pad 
const int BTN_STRIKE      = 39; 
const int BTN_AUTO_CHORD  = 40;
const int BTN_LFO_REPEAT  = 41;
const int BTN_ENV_SHAPE   = 42;

// Group B: Core Engine 
const int BTN_DRONE       = 17;
const int BTN_LFO_TARGET  = 18;
const int BTN_GLIDE       = 21;
const int BTN_SCALE       = 38;

// Group C: Oh Sh*t Modifiers
const int BTN_FREEZE      = 45;
const int BTN_SUB_DROP    = 47;
const int BTN_SCRAMBLE    = 48;

// Encoder
const int ENC_A = 14;
const int ENC_B = 15;
const int ENC_SW = 16;

Bounce2::Button buttons[11];
const int BUTTON_PINS[11] = {
    BTN_STRIKE, BTN_AUTO_CHORD, BTN_LFO_REPEAT, BTN_ENV_SHAPE,
    BTN_DRONE, BTN_LFO_TARGET, BTN_GLIDE, BTN_SCALE,
    BTN_FREEZE, BTN_SUB_DROP, BTN_SCRAMBLE
};

Bounce2::Button btnEnc = Bounce2::Button();

volatile int encoderPos = 0;
volatile int lastEncoded = 0;

void IRAM_ATTR encoderISR() {
    int MSB = digitalRead(ENC_A);
    int LSB = digitalRead(ENC_B);
    int encoded = (MSB << 1) | LSB;
    int sum  = (lastEncoded << 2) | encoded;

    // Fixed volatile increments for C++ standard compliance
    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos = encoderPos + 1;
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos = encoderPos - 1;
    
    lastEncoded = encoded;
}

// --- NEW: Mux Reading Helper ---
int readMux(int channel) {
    // Write binary logic to select pins
    digitalWrite(MUX_S0, bitRead(channel, 0));
    digitalWrite(MUX_S1, bitRead(channel, 1));
    digitalWrite(MUX_S2, bitRead(channel, 2));
    
    // Crucial anti-crosstalk delay for the ESP32 ADC capacitor
    delayMicroseconds(10); 
    
    return analogRead(MUX_Z);
}

void initUI() {
    // Initialize Mux pins
    pinMode(MUX_Z, INPUT);
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    
    for (int i = 0; i < 11; i++) {
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

void uiTask(void *pvParameters) {
    initUI();
    int lastEncPos = 0;
    
    // Auto-Chord tracking variables
    int lastPitch1 = -1, lastPitch2 = -1, lastPitch3 = -1;

    // Boot Defaults
    state.rootNote = 36; 
    state.activeBank = 0;
    state.droneMode = true;
    state.envShape = 0;
    state.lfoTarget = 0;
    state.glideSpeed = 0;
    state.activeScale = 0;

    for(;;) {
        // --- 1. Read Pots via 4051 Mux (Channels 0-6) ---
        int p1 = map(readMux(0), 0, 4095, 0, 24); // Osc 1 Pitch
        int p2 = map(readMux(1), 0, 4095, 0, 24); // Osc 2 Pitch
        int p3 = map(readMux(2), 0, 4095, 0, 24); // Osc 3 Pitch

        // Auto-Chord Triggering Logic
        if (state.autoChord && !state.droneMode) {
            if (p1 != lastPitch1 || p2 != lastPitch2 || p3 != lastPitch3) {
                state.triggerStab = true; 
            }
        }
        
        state.osc1Pitch = p1; state.osc2Pitch = p2; state.osc3Pitch = p3;
        lastPitch1 = p1; lastPitch2 = p2; lastPitch3 = p3;

        state.wavetableMorph = map(readMux(3), 0, 4095, 0, 7); 
        state.filterCutoff = readMux(4);
        state.filterRes = readMux(5);
        state.washMix = readMux(6);

        // --- 2. Update All Buttons ---
        for (int i = 0; i < 11; i++) buttons[i].update();
        btnEnc.update();

        // --- 3. Process Button Presses ---
        
        // Group A: Trigger Pad
        if (buttons[0].pressed() && !state.droneMode) state.triggerStab = true; 
        if (buttons[1].pressed()) state.autoChord = !state.autoChord;           
        if (buttons[2].pressed()) state.lfoRepeater = !state.lfoRepeater;       
        if (buttons[3].pressed()) state.envShape = (state.envShape + 1) % 3;    

        // Group B: Core Engine
        if (buttons[4].pressed()) state.droneMode = !state.droneMode;           
        if (buttons[5].pressed()) state.lfoTarget = (state.lfoTarget + 1) % 4;  
        if (buttons[6].pressed()) state.glideSpeed = (state.glideSpeed + 1) % 3;
        if (buttons[7].pressed()) state.activeScale = (state.activeScale + 1) % 4; 

        // Group C: Modifiers
        state.washFreeze = buttons[8].isPressed();  
        state.subBassDrop = buttons[9].isPressed(); 
        
        if (buttons[10].pressed()) { // SCRAMBLE!
            state.activeBank = random(0, 3);
            state.lfoTarget = random(0, 4);
            state.envShape = random(0, 3);
            state.activeScale = random(0, 4);
            state.glideSpeed = random(0, 3);
        }

        // --- 4. Encoder Logic ---
        if (btnEnc.pressed()) state.activeBank = (state.activeBank + 1) % 3;
        
        if (encoderPos > lastEncPos) {
            state.rootNote = min(84, state.rootNote + 1);
            lastEncPos = encoderPos;
        } else if (encoderPos < lastEncPos) {
            state.rootNote = max(24, state.rootNote - 1);
            lastEncPos = encoderPos;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}