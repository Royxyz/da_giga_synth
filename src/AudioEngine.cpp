#include "AudioEngine.h"
#include "GlobalState.h"
#include "I2SOutput.h" // The new wrapper

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

const int16_t* const BANK_TABLE[3] = { BANK_ANALOG, BANK_GROWL, BANK_FM };

VoiceManager synthVoices(BANK_TABLE[0], BANK_TABLE[0], 2048, 16, AUDIO_RATE); 
FloatSVF mainFilter;
FloatLFO masterLFO(AUDIO_RATE / CONTROL_RATE_DIVIDER); 
FloatEnvelope modEnv(AUDIO_RATE); 
PsramAbyss theAbyss;


float currentTargetMorph1 = 0.0f;
float currentTargetMorph2 = 0.0f;
float currentTargetCutoff = 2000.0f;

void setupAudioEngine() {
    if (!theAbyss.init()) {
        Serial.println("FATAL: PSRAM Allocation Failed!");
    }
    if (!dac.begin(I2S_BCK, I2S_WS, I2S_DATA, (uint32_t)AUDIO_RATE)) {
        Serial.println("FATAL: I2S DAC Initialization Failed!");
    }
}

void updateControl() {
    float baseMorph1 = state.morph1.load();
    float baseMorph2 = state.morph2.load();
    float baseCutoff = state.filterCutoff.load();
    float modDepth   = state.modDepth.load();
    int mTarget      = state.modTarget.load();
    
    masterLFO.setRate(state.lfoRate.load());
    float lfoSignal = masterLFO.process(state.lfoWave.load()); 

    currentTargetCutoff = baseCutoff;
    currentTargetMorph1 = baseMorph1;
    currentTargetMorph2 = baseMorph2;

    if (!state.useModEnv.load()) {
        if (mTarget == 0) { 
            currentTargetCutoff += (lfoSignal * modDepth * 5000.0f);
            if (currentTargetCutoff < 20.0f) currentTargetCutoff = 20.0f;
            if (currentTargetCutoff > 20000.0f) currentTargetCutoff = 20000.0f;
        } 
        else if (mTarget == 1) { 
            currentTargetMorph1 += (lfoSignal * modDepth * 7.0f);
            if (currentTargetMorph1 > 7.0f) currentTargetMorph1 = 7.0f;
            if (currentTargetMorph1 < 0.0f) currentTargetMorph1 = 0.0f;
        }
        else if (mTarget == 2) { 
            currentTargetMorph2 += (lfoSignal * modDepth * 7.0f);
            if (currentTargetMorph2 > 7.0f) currentTargetMorph2 = 7.0f;
            if (currentTargetMorph2 < 0.0f) currentTargetMorph2 = 0.0f;
        }
    }
    
    synthVoices.setBanks(BANK_TABLE[state.osc1Bank.load()], BANK_TABLE[state.osc2Bank.load()]);
    synthVoices.setSync(state.oscSync.load());
}

int16_t updateAudio() {
    float audioRateMorph1 = currentTargetMorph1;
    float audioRateMorph2 = currentTargetMorph2;
    float audioRateCutoff = currentTargetCutoff;

    if (state.useModEnv.load()) {
        float envSignal = modEnv.process();
        float depth = state.modDepth.load();
        int target = state.modTarget.load();
        
        if (target == 0) {
            audioRateCutoff += (envSignal * depth * 5000.0f);
            if (audioRateCutoff > 20000.0f) audioRateCutoff = 20000.0f;
        } else if (target == 1) {
            audioRateMorph1 += (envSignal * depth * 7.0f);
            if (audioRateMorph1 > 7.0f) audioRateMorph1 = 7.0f;
        } else if (target == 2) {
            audioRateMorph2 += (envSignal * depth * 7.0f);
            if (audioRateMorph2 > 7.0f) audioRateMorph2 = 7.0f;
        }
    }

    float rawMix = synthVoices.process(audioRateMorph1, audioRateMorph2);

    mainFilter.setCutoffRes(audioRateCutoff, state.filterRes.load());
    mainFilter.process(rawMix); 
    
    float filteredOut = mainFilter.lp;
    int fMode = state.filterMode.load();
    if (fMode == 1) filteredOut = mainFilter.bp;
    if (fMode == 2) filteredOut = mainFilter.hp;

    float finalMix = theAbyss.process(
        filteredOut, 
        state.fxMix.load(), 
        state.fxMode.load(), 
        state.fxFreeze.load()
    );

    return (int16_t)(finalMix * 32760.0f); 
}

// --- The FreeRTOS Audio Task ---
void audioTask(void *pvParameters) {
    setupAudioEngine();
    
    int controlCounter = 0; 

    for(;;) {
        if (controlCounter++ >= CONTROL_RATE_DIVIDER) {
            updateControl();
            controlCounter = 0;
        }

        int16_t out = updateAudio();
        
        dac.writeSample(out, out); 
    }
}

void engineNoteOn(uint8_t note, uint8_t velocity) {
    modEnv.noteOn(); 
    synthVoices.noteOn(note, state.detune2.load(), state.osc2OctaveDrop.load());
}

void engineNoteOff(uint8_t note, uint8_t velocity) {
    modEnv.noteOff();
    synthVoices.noteOff(note);
}