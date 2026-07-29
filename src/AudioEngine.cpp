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

TuringMachine turingSeq;
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> aOsc(SAW2048_DATA);
ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE> envelope;
ResonantFilter<LOWPASS> lpf; 

EventDelay stepTimer;
float currentFreqHz = 0.0f; 

// --- PSRAM CLOUD SETUP ---
// 131,072 samples = exactly 4.0 seconds at 32768Hz (requires ~262KB PSRAM)
#define CLOUD_BUFFER_SIZE 131072 
int16_t* psramCloudBuffer = NULL;
uint32_t cloudWriteHead = 0;

void setupAudioEngine() {
    stepTimer.set(250);
    envelope.setADLevels(255, 255);
    envelope.setAttackTime(10);
    envelope.setReleaseTime(10);
    
    // Allocate the massive delay line cleanly into the PSRAM heap
    psramCloudBuffer = (int16_t*)ps_malloc(CLOUD_BUFFER_SIZE * sizeof(int16_t));
    if (psramCloudBuffer != NULL) {
        // Zero it out so we don't blast garbage data on boot
        memset(psramCloudBuffer, 0, CLOUD_BUFFER_SIZE * sizeof(int16_t));
    }
    
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
        
        if (turingSeq.isTrigger()) {
            envelope.setDecayLevel(0);
            envelope.setDecayTime(map(state.envDecay, 0, 4095, 50, 1500));
            envelope.noteOn();
        }
    }
    
    float midiNote = turingSeq.getCurrentMidiNote(state.scaleType) + (state.octaveOffset * 12.0f);
    if (midiNote < 0) midiNote = 0;
    if (midiNote > 127) midiNote = 127;
    
    float targetFreqHz = mtof((uint8_t)midiNote);
    float glide = state.glide / 4178.0f; 
    currentFreqHz = (currentFreqHz * glide) + (targetFreqHz * (1.0f - glide));
    
    uint32_t freq_Q16n16 = (uint32_t)(currentFreqHz * 65536.0f);
    aOsc.setFreq_Q16n16(freq_Q16n16);

    envelope.update();
    
    uint8_t cutoff = map(state.filterCutoff, 0, 4095, 10, 255);
    uint8_t res = map(state.filterRes, 0, 4095, 0, 255);
    lpf.setCutoffFreqAndResonance(cutoff, res);
}

AudioOutput updateAudio() {
    // 1. Generate core synth signal
    int oscSample = aOsc.next(); 
    oscSample = lpf.next(oscSample); 
    
    // We apply the envelope FIRST. If we apply it after the delay, 
    // the massive cloud trail will violently mute the instant the note ends.
    int envOutput = oscSample * envelope.next(); 
    
    int finalOutL = envOutput;
    int finalOutR = envOutput;

    // 2. Apply PSRAM Cloud FX
    if (state.activeFX == 1 && psramCloudBuffer != NULL) {
        // Read 4 different taps spread across the 4-second buffer to smear the audio
        int tap1 = psramCloudBuffer[(cloudWriteHead - 16384 + CLOUD_BUFFER_SIZE) % CLOUD_BUFFER_SIZE];  // ~0.5s
        int tap2 = psramCloudBuffer[(cloudWriteHead - 40000 + CLOUD_BUFFER_SIZE) % CLOUD_BUFFER_SIZE];  // ~1.2s
        int tap3 = psramCloudBuffer[(cloudWriteHead - 85000 + CLOUD_BUFFER_SIZE) % CLOUD_BUFFER_SIZE];  // ~2.6s
        int tap4 = psramCloudBuffer[(cloudWriteHead - 131000 + CLOUD_BUFFER_SIZE) % CLOUD_BUFFER_SIZE]; // ~4.0s

        // Mix down the taps to prevent brutal clipping
        int cloudOut = (tap1 + tap2 + tap3 + tap4) >> 2;

        // Feedback loop feeding the dry signal + a heavily attenuated longest tap back into PSRAM
        int feedbackSample = envOutput + ((tap4 * 210) >> 8); // ~75% feedback

        // Hard clamp to 16-bit limits to prevent integer rollover screeching
        if (feedbackSample > 32767) feedbackSample = 32767;
        if (feedbackSample < -32768) feedbackSample = -32768;

        psramCloudBuffer[cloudWriteHead] = feedbackSample;
        cloudWriteHead = (cloudWriteHead + 1) % CLOUD_BUFFER_SIZE;

        // Blend 50/50 with the dry signal for output
        finalOutL = (envOutput + cloudOut) >> 1;
        finalOutR = finalOutL;
    }

    return StereoOutput::from16Bit(finalOutL, finalOutR); 
}

void audioLoopWrapper() {
    audioHook();
}