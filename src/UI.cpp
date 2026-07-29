#include "UI.h"
#include "GlobalState.h"
#include <Bounce2.h>

const int POT_PINS[7] = {7, 8, 9, 10, 11, 12, 13};

// Group A: Trigger Pad (New Pins)
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

    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos = encoderPos + 1;
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos = encoderPos - 1;
    
    lastEncoded = encoded;
}

void initUI() {
    for (int i = 0; i < 7; i++) pinMode(POT_PINS[i], INPUT);
    
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
        // --- 1. Read Pots & Auto-Chord Logic ---
        int p1 = map(analogRead(POT_PINS[0]), 0, 4095, 0, 24);
        int p2 = map(analogRead(POT_PINS[1]), 0, 4095, 0, 24);
        int p3 = map(analogRead(POT_PINS[2]), 0, 4095, 0, 24);

        if (state.autoChord && !state.droneMode) {
            if (p1 != lastPitch1 || p2 != lastPitch2 || p3 != lastPitch3) {
                state.triggerStab = true; 
            }
        }
        
        state.osc1Pitch = p1; state.osc2Pitch = p2; state.osc3Pitch = p3;
        lastPitch1 = p1; lastPitch2 = p2; lastPitch3 = p3;

        state.wavetableMorph = map(analogRead(POT_PINS[3]), 0, 4095, 0, 7); 
        state.filterCutoff = analogRead(POT_PINS[4]);
        state.filterRes = analogRead(POT_PINS[5]);
        state.washMix = analogRead(POT_PINS[6]);

        // --- 2. Update All Buttons ---
        for (int i = 0; i < 11; i++) buttons[i].update();
        btnEnc.update();

        // --- 3. Process Button Presses ---
        
        // Group A: Trigger Pad
        if (buttons[0].pressed() && !state.droneMode) state.triggerStab = true; // Strike
        if (buttons[1].pressed()) state.autoChord = !state.autoChord;           // Auto-Chord
        if (buttons[2].pressed()) state.lfoRepeater = !state.lfoRepeater;       // Repeater
        if (buttons[3].pressed()) state.envShape = (state.envShape + 1) % 3;    // Env Shape

        // Group B: Core Engine
        if (buttons[4].pressed()) state.droneMode = !state.droneMode;           // Drone
        if (buttons[5].pressed()) state.lfoTarget = (state.lfoTarget + 1) % 4;  // LFO Target
        if (buttons[6].pressed()) state.glideSpeed = (state.glideSpeed + 1) % 3;// Glide
        if (buttons[7].pressed()) state.activeScale = (state.activeScale + 1) % 4; // Scale

        // Group C: Modifiers
        state.washFreeze = buttons[8].isPressed();  // Momentary Freeze (Hold to loop)
        state.subBassDrop = buttons[9].isPressed(); // Momentary Sub-Bass (Hold to drop)
        
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