#include "UI.h"
#include "GlobalState.h"
#include <Bounce2.h>

const int POT_PINS[7] = {7, 8, 9, 10, 11, 12, 13};
const int BTN_LOCK = 17;
const int BTN_MUTATE = 18;
const int BTN_OCT_DOWN = 21; 
const int BTN_OCT_UP = 38;   
const int ENC_A = 14;
const int ENC_B = 15;
const int ENC_SW = 16;

// The new Wash button on Pin 45
const int BTN_FX_WASH = 45;  

Bounce2::Button btnLock = Bounce2::Button();
Bounce2::Button btnMutate = Bounce2::Button();
Bounce2::Button btnOctDown = Bounce2::Button(); 
Bounce2::Button btnOctUp = Bounce2::Button();   
Bounce2::Button btnEnc = Bounce2::Button();
Bounce2::Button btnWash = Bounce2::Button();

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

void initUI() {
    for (int i = 0; i < 7; i++) {
        pinMode(POT_PINS[i], INPUT);
    }
    
    btnLock.attach(BTN_LOCK, INPUT_PULLUP);
    btnLock.interval(5);
    btnLock.setPressedState(LOW);

    btnMutate.attach(BTN_MUTATE, INPUT_PULLUP);
    btnMutate.interval(5);
    btnMutate.setPressedState(LOW);
    
    btnOctDown.attach(BTN_OCT_DOWN, INPUT_PULLUP); 
    btnOctDown.interval(5);
    btnOctDown.setPressedState(LOW);

    btnOctUp.attach(BTN_OCT_UP, INPUT_PULLUP);     
    btnOctUp.interval(5);
    btnOctUp.setPressedState(LOW);
    
    btnEnc.attach(ENC_SW, INPUT_PULLUP);
    btnEnc.interval(5);
    btnEnc.setPressedState(LOW);

    btnWash.attach(BTN_FX_WASH, INPUT_PULLUP);
    btnWash.interval(5);
    btnWash.setPressedState(LOW);

    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B), encoderISR, CHANGE);
}

void uiTask(void *pvParameters) {
    initUI();
    int lastEncPos = 0;

    for(;;) {
        state.tempo = analogRead(POT_PINS[0]);
        state.probability = analogRead(POT_PINS[1]);
        state.sequenceLength = analogRead(POT_PINS[2]);
        state.glide = analogRead(POT_PINS[3]);
        state.filterCutoff = analogRead(POT_PINS[4]);
        state.filterRes = analogRead(POT_PINS[5]);
        state.envDecay = analogRead(POT_PINS[6]);

        btnLock.update();
        btnMutate.update();
        btnOctDown.update(); 
        btnOctUp.update();   
        btnEnc.update();
        btnWash.update();

        if (btnLock.pressed()) state.lockSequence = !state.lockSequence;
        state.forceMutate = btnMutate.isPressed();
        
        if (btnOctDown.pressed() && state.octaveOffset > -3) {
            state.octaveOffset--;
        }
        if (btnOctUp.pressed() && state.octaveOffset < 3) {
            state.octaveOffset++;
        }

        if (btnWash.pressed()) {
            state.activeFX = (state.activeFX == 1) ? 0 : 1; 
        }
        
        if (encoderPos > lastEncPos) {
            state.scaleType++;
            lastEncPos = encoderPos;
        } else if (encoderPos < lastEncPos) {
            state.scaleType--;
            if (state.scaleType < 0) state.scaleType = 0;
            lastEncPos = encoderPos;
        }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}