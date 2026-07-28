#include "AudioEngine.h"
#include "GlobalState.h"
#include "TuringMachine.h"

#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_AUDIO_CHANNELS 2
#define MOZZI_AUDIO_RATE 32768
#define MOZZI_CONTROL_RATE 256
#define MOZZI_I2S_PIN_BCK 4
#define MOZZI_I2S_PIN_WS 5     
#define MOZZI_I2S_PIN_DATA 6

#include <Mozzi.h>
#include <Oscil.h>
#include <tables/saw2048_int8.h> 
#include <mozzi_midi.h>          
#include <EventDelay.h>          
#include <Smooth.h>              
#include <ADSR.h>
#include <ResonantFilter.h>

TuringMachine turingSeq;
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> aOsc(SAW2048_DATA);
ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE> envelope;
ResonantFilter<LOWPASS, uint16_t> lpf; // Swapped to ResonantFilter
EventDelay stepTimer;
Smooth<int> pitchGlide(0.95f);

int currentTargetFreq = 0; 


void updateControl() {
    if (stepTimer.ready()) {
        unsigned int stepDelay = map(state.tempo, 0, 4095, 50, 600);
        stepTimer.set(stepDelay);
        stepTimer.start(); 

        int prob = map(state.probability, 0, 4095, 0, 255);
        int len = map(state.sequenceLength, 0, 4095, 1, 16);

        turingSeq.advanceStep(prob, len, state.forceMutate);
        float midiNote = turingSeq.getCurrentMidiNote(state.scaleType);
        
        currentTargetFreq = mtof(midiNote) * 256;
        
        envelope.setDecayLevel(0);
        envelope.setDecayTime(map(state.envDecay, 0, 4095, 10, 1000));
        envelope.noteOn();
    }

    envelope.update();
    
    float glideFactor = map(state.glide, 0, 4095, 0, 99) / 100.0f;
    pitchGlide.setSmoothness(glideFactor);
    int smoothedFreq = pitchGlide.next(currentTargetFreq);
    aOsc.setFreq_Q16n16(smoothedFreq);

    // ResonantFilter expects uint8_t for both cutoff (0-255) and resonance (0-255)
    uint8_t cutoff = map(state.filterCutoff, 0, 4095, 0, 255);
    uint8_t res = map(state.filterRes, 0, 4095, 0, 255);
    lpf.setCutoffFreqAndResonance(cutoff, res);
}

AudioOutput updateAudio() {
    int currentSample = aOsc.next();
    currentSample = lpf.next(currentSample);
    
    // Apply VCA envelope (shift right to scale back down to 16-bit range)
    int envOutput = (currentSample * envelope.next()) >> 8; 
    
    // Return StereoOutput to match MOZZI_AUDIO_CHANNELS STEREO
    return StereoOutput::from16Bit(envOutput, envOutput); 
}

void audioTask(void *pvParameters) {
    stepTimer.set(250);
    envelope.setADLevels(255, 255);
    envelope.setAttackTime(10);
    envelope.setReleaseTime(10);
    startMozzi();
    
    for(;;) {
        audioHook(); 
    }
}