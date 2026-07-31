#include "AudioEngine.h"
#include "GlobalState.h"

#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_AUDIO_CHANNELS MOZZI_STEREO 
#define MOZZI_AUDIO_RATE 32768
#define MOZZI_CONTROL_RATE 256
#define MOZZI_I2S_PIN_BCK 4
#define MOZZI_I2S_PIN_WS 5     
#define MOZZI_I2S_PIN_DATA 6

#include <Mozzi.h>

#include "VoiceManager.h"
#include "FloatSVF.h"
#include "FloatLFO.h"
#include "PsramAbyss.h"
#include "FloatEnvelope.h"


#include "BankAnalog.h" 
#include "BankGrowl.h"
#include "BankFM.h"


const int16_t* const BANK_TABLE[3] = {
    BANK_ANALOG, 
    BANK_GROWL,
    BANK_FM
};

VoiceManager synthVoices(BANK_TABLE[0], BANK_TABLE[0], 2048, 16);

FloatSVF mainFilter;
FloatLFO masterLFO(32768.0f);
FloatEnvelope modEnv(32768.0f); 
PsramAbyss theAbyss;

void setupAudioEngine() {
    if (!theAbyss.init()) {
        Serial.println("FATAL: PSRAM Allocation Failed for The Abyss!");
    }
    startMozzi();
}

void updateControl() {}

AudioOutput updateAudio() {
    float curMorph1 = state.morph1.load();
    float curMorph2 = state.morph2.load();
    float curCutoff = state.filterCutoff.load();
    float curRes    = state.filterRes.load();
    float modDepth  = state.modDepth.load();
    float fxMix     = state.fxMix.load();
    
    int b1 = state.osc1Bank.load();
    int b2 = state.osc2Bank.load();
    int mTarget = state.modTarget.load();
    bool useMod = state.useModEnv.load();

//Update Wavetable Pointers 
    synthVoices.setBanks(BANK_TABLE[b1], BANK_TABLE[b2]);
    synthVoices.setSync(state.oscSync.load());

//Process Modulators 
    masterLFO.setRate(state.lfoRate.load());
    
  
    float modSignal = 0.0f;
    if (useMod) {
        modSignal = modEnv.process(); 
    } else {
        modSignal = masterLFO.process(state.lfoWave.load()); 
    }

//Apply Modulation Matrix 
    if (mTarget == 0) { 
        curCutoff += (modSignal * modDepth * 5000.0f);
        if (curCutoff < 20.0f) curCutoff = 20.0f;
        if (curCutoff > 20000.0f) curCutoff = 20000.0f;
    } 
    else if (mTarget == 1) { 
        curMorph1 += (modSignal * modDepth * 7.0f);
        if (curMorph1 < 0.0f) curMorph1 = 0.0f;
        if (curMorph1 > 7.0f) curMorph1 = 7.0f;
    }
    else if (mTarget == 2) { 
        curMorph2 += (modSignal * modDepth * 7.0f);
        if (curMorph2 < 0.0f) curMorph2 = 0.0f;
        if (curMorph2 > 7.0f) curMorph2 = 7.0f;
    }

//Render Active Polyphony
    float rawMix = synthVoices.process(curMorph1, curMorph2);

//Master Filter
    mainFilter.setCutoffRes(curCutoff, curRes);
    mainFilter.process(rawMix); 
    
    float filteredOut = mainFilter.lp;
    int fMode = state.filterMode.load();
    if (fMode == 1) filteredOut = mainFilter.bp;
    if (fMode == 2) filteredOut = mainFilter.hp;

//PSRAM FX
    float finalMix = theAbyss.process(
        filteredOut, 
        fxMix, 
        state.fxMode.load(), 
        state.fxFreeze.load()
    );

//Output Scaling 

    int16_t finalAudio = (int16_t)(finalMix * 32760.0f); 

    return StereoOutput::from16Bit(finalAudio, finalAudio); 
}

void audioLoopWrapper() {
    audioHook();
}

void engineNoteOn(uint8_t note, uint8_t velocity) {
    float detune = state.detune2.load();
    int octDrop = state.osc2OctaveDrop.load();
    
    synthVoices.noteOn(note, detune, octDrop);
}

void engineNoteOff(uint8_t note, uint8_t velocity) {
    synthVoices.noteOff(note);
}