#include "AudioEngine.h"
#include "GlobalState.h"
#include "I2SOutput.h" 
#include "VoiceManager.h"
#include "FloatSVF.h"
#include "FloatLFO.h"
#include "PsramAbyss.h"
#include "FloatEnvelope.h"
#include "BankAnalog.h" 
#include "BankGrowl.h"
#include "BankFM.h"

const int I2S_BCK = 4;
const int I2S_WS = 5;
const int I2S_DATA = 6;
const float AUDIO_RATE = 48000.0f;
const int CONTROL_RATE_DIVIDER = 46; 

I2SOutput dac;

// 16-bit arrays
const int16_t* const BANK_TABLE[3] = { BANK_ANALOG, BANK_GROWL, BANK_FM };

// INITIALIZED TO 4096 TO MATCH 88.2KHZ RIPS
VoiceManager synthVoices(BANK_TABLE[0], 4096, 16, AUDIO_RATE); 
FloatSVF mainFilter;
FloatLFO masterLFO(AUDIO_RATE / CONTROL_RATE_DIVIDER); 
FloatEnvelope modEnv(AUDIO_RATE); 
PsramAbyss theAbyss;

// 16-BIT BUFFER OVERHAUL
const int AUDIO_BUFFER_SIZE = 64;
int16_t i2sBuffer[AUDIO_BUFFER_SIZE * 2]; 

float currentTargetMorph1 = 0.0f;
float currentTargetCutoff = 2000.0f;

float cachedFilterRes;
int cachedFilterMode;
float cachedFxMix;
int cachedFxMode;
bool cachedFxFreeze;

void setupAudioEngine() {
    if (!theAbyss.init()) Serial.println("FATAL: PSRAM Allocation Failed!");
    if (!dac.begin(I2S_BCK, I2S_WS, I2S_DATA, (uint32_t)AUDIO_RATE)) {
        Serial.println("FATAL: I2S DAC Initialization Failed!");
    }
}

void updateControl() {
    float baseMorph1 = state.morph1.load();
    float baseCutoff = state.filterCutoff.load();
    float modDepth   = state.modDepth.load();
    int mTarget      = state.modTarget.load();
    
    cachedFilterRes  = state.filterRes.load();
    cachedFilterMode = state.filterMode.load();
    cachedFxMix      = state.fxMix.load();
    cachedFxMode     = state.fxMode.load();
    cachedFxFreeze   = state.fxFreeze.load();
    
    synthVoices.setEnvelopes(state.envAttack.load(), 100.0f, 0.8f, state.envRelease.load());
    synthVoices.setBank(BANK_TABLE[state.osc1Bank.load()]);

    float modSignal = 0.0f;
    if (state.useModEnv.load()) {
        modSignal = modEnv.process();
    } else {
        masterLFO.setRate(state.lfoRate.load());
        modSignal = masterLFO.process(state.lfoWave.load()); 
    }

    currentTargetCutoff = baseCutoff;
    currentTargetMorph1 = baseMorph1;

    if (mTarget == 0) { 
        currentTargetCutoff += (modSignal * modDepth * 5000.0f);
        if (currentTargetCutoff < 20.0f) currentTargetCutoff = 20.0f;
        if (currentTargetCutoff > 20000.0f) currentTargetCutoff = 20000.0f;
    } 
    else if (mTarget == 1) { 
        currentTargetMorph1 += (modSignal * modDepth * 7.0f);
        if (currentTargetMorph1 > 7.0f) currentTargetMorph1 = 7.0f;
        if (currentTargetMorph1 < 0.0f) currentTargetMorph1 = 0.0f;
    }

    mainFilter.setCutoffRes(currentTargetCutoff, cachedFilterRes);
}

// RESTORED TO RETURN INT16
int16_t updateAudio() {
    float rawMix = synthVoices.process(currentTargetMorph1);
    
    mainFilter.process(rawMix); 
    
    float filteredOut = mainFilter.lp;
    if (cachedFilterMode == 1) filteredOut = mainFilter.bp;
    if (cachedFilterMode == 2) filteredOut = mainFilter.hp;

    float finalMix = theAbyss.process(
        filteredOut, 
        cachedFxMix, 
        cachedFxMode, 
        cachedFxFreeze
    );

    float safeMix = tanhf(finalMix);
    return (int16_t)(safeMix * 32760.0f); 
}

void audioTask(void *pvParameters) {
    setupAudioEngine();
    
    int controlCounter = 0; 
    size_t bytesWritten;
    
    // Start at 500 so it DOES NOT print on boot
    static int printCount = 500; 

    for(;;) {
        // --- NEW: Wait for Python to send a character ---
        if (Serial.available()) {
            while(Serial.available()) Serial.read(); // Clear the incoming buffer
            printCount = 0; // Resetting to 0 triggers the 500-sample dump
        }

        for(int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
            
            if (controlCounter++ >= CONTROL_RATE_DIVIDER) {
                updateControl();
                controlCounter = 0;
            }

            int16_t out = updateAudio();
            
            // Only prints when Python asks for it
            if (printCount < 5000) {
                Serial.println(out);
                printCount++;
            }

            i2sBuffer[i * 2] = out;     
            i2sBuffer[(i * 2) + 1] = out; 
        }

        i2s_channel_write(dac.tx_chan, i2sBuffer, sizeof(i2sBuffer), &bytesWritten, portMAX_DELAY);
    }
}

void engineNoteOn(uint8_t note, uint8_t velocity) {
    modEnv.noteOn(); 
    synthVoices.noteOn(note);
}

void engineNoteOff(uint8_t note, uint8_t velocity) {
    modEnv.noteOff();
    synthVoices.noteOff(note);
}