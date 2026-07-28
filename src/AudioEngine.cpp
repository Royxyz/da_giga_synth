#include "AudioEngine.h"
#include "GlobalState.h"
#include "TuringMachine.h"

#include <MozziConfigValues.h> 

#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_AUDIO_CHANNELS MOZZI_STEREO 
#define MOZZI_AUDIO_RATE 32768
#define MOZZI_CONTROL_RATE 256
#define MOZZI_I2S_PIN_BCK 4
#define MOZZI_I2S_PIN_WS 5     
#define MOZZI_I2S_PIN_DATA 6

#include <Mozzi.h>
#include <Oscil.h>
#include <tables/saw2048_int8.h> 
#include <EventDelay.h>          
#include <ADSR.h>
#include <ResonantFilter.h>
#include <mozzi_midi.h> 
#include <AudioDelay.h>

TuringMachine turingSeq;
ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE> envelope;

// Oscillators
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> aOsc(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> oscDetuneL(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> oscDetuneR(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> oscFifth(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> oscOctave(SAW2048_DATA);

// Dual filters for true stereo processing during Unison mode
ResonantFilter<LOWPASS> lpfL; 
ResonantFilter<LOWPASS> lpfR; 

AudioDelay<16384, int> washDelay; 

EventDelay stepTimer;
float currentFreqHz = 0.0f; 

// Bulletproof clipping prevention for heavy mixed signals
inline int clamp16(int sample) {
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return sample;
}

void setupAudioEngine() {
    stepTimer.set(250);
    envelope.setADLevels(255, 255);
    envelope.setAttackTime(10);
    envelope.setReleaseTime(10);
    startMozzi();
}

void updateControl() {
    if (stepTimer.ready()) {
        unsigned int stepDelay = map(state.tempo, 0, 4095, 100, 1000);
        stepTimer.set(stepDelay);
        stepTimer.start(); 

        int prob = map(state.probability, 0, 4095, 0, 255);
        int len = map(state.sequenceLength, 0, 4095, 1, 16);

        turingSeq.advanceStep(prob, len, state.forceMutate);
        
        envelope.setDecayLevel(0);
        envelope.setDecayTime(map(state.envDecay, 0, 4095, 50, 1500));
        envelope.noteOn();
    }
    
    float midiNote = turingSeq.getCurrentMidiNote(state.scaleType) + (state.octaveOffset * 12.0f);
    if (midiNote < 0) midiNote = 0;
    if (midiNote > 127) midiNote = 127;
    
    float targetFreqHz = mtof((uint8_t)midiNote);
    float glide = state.glide / 4178.0f; 
    currentFreqHz = (currentFreqHz * glide) + (targetFreqHz * (1.0f - glide));
    
    // Set Main Oscillator
    aOsc.setFreq_Q16n16((uint32_t)(currentFreqHz * 65536.0f));

    // Set Unison Oscillators (~1% detune mapping to approx 17 cents)
    oscDetuneL.setFreq_Q16n16((uint32_t)(currentFreqHz * 0.99f * 65536.0f));
    oscDetuneR.setFreq_Q16n16((uint32_t)(currentFreqHz * 1.01f * 65536.0f));

    // Set Chord Oscillators (+7 semitones = 1.4983x, +12 semitones = 2.0x)
    oscFifth.setFreq_Q16n16((uint32_t)(currentFreqHz * 1.4983f * 65536.0f));
    oscOctave.setFreq_Q16n16((uint32_t)(currentFreqHz * 2.0f * 65536.0f));

    envelope.update();
    
    uint8_t cutoff = map(state.filterCutoff, 0, 4095, 10, 255);
    uint8_t res = map(state.filterRes, 0, 4095, 0, 255);
    
    lpfL.setCutoffFreqAndResonance(cutoff, res);
    lpfR.setCutoffFreqAndResonance(cutoff, res);
}

AudioOutput updateAudio() {
    int mainOsc = aOsc.next(); 
    int envVal = envelope.next();
    int outL = 0;
    int outR = 0;

    if (state.activeFX == 0) {
        // BYPASS: Standard Mono Route
        int filtered = lpfL.next(mainOsc);
        outL = filtered * envVal;
        outR = outL;
    } 
    else if (state.activeFX == 1) {
        // FX 1: TAPE WASH
        int filtered = lpfL.next(mainOsc);
        int finalAudio = filtered * envVal;
        
        static int lastWashOut = 0;
        // Divide input by 2 to prevent delay line buildup explosion
        int delayIn = (finalAudio >> 1) + ((lastWashOut * 210) >> 8); 
        lastWashOut = washDelay.next(delayIn);
        
        outL = finalAudio + lastWashOut;
        outR = outL;
    }
    else if (state.activeFX == 2) {
        // FX 2: TRANCE UNISON (True Stereo)
        int rawL = (mainOsc + oscDetuneL.next()) >> 1;
        int rawR = (mainOsc + oscDetuneR.next()) >> 1;
        
        // Filter independently to maintain stereo separation
        outL = lpfL.next(rawL) * envVal;
        outR = lpfR.next(rawR) * envVal;
    }
    else if (state.activeFX == 3) {
        // FX 3: RAVE CHORD 
        int rawChord = (mainOsc + oscFifth.next() + oscOctave.next()) / 3;
        int filtered = lpfL.next(rawChord);
        
        outL = filtered * envVal;
        outR = outL;
    }

    return StereoOutput::from16Bit(clamp16(outL), clamp16(outR)); 
}

void audioLoopWrapper() {
    audioHook();
}