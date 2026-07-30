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
#include <Oscil16.h>
#include <Oscil.h>
#include <ADSR.h>
#include <ResonantFilter.h>
#include <EventDelay.h>
#include <mozzi_midi.h> 

// LFO Table for modulation
#include <tables/sin2048_int8.h>

// Your Python-Generated 16-bit Banks
#include "BankAnalog.h" 
#include "BankGrowl.h"
#include "BankFM.h"

// Cast pointers to  to satisfy Mozzi's constructor constraints
Oscil16<2048, MOZZI_AUDIO_RATE> osc1((const int16_t*)BANK_ANALOG[0]);
Oscil16<2048, MOZZI_AUDIO_RATE> osc2((const int16_t*)BANK_GROWL[0]);
Oscil16<2048, MOZZI_AUDIO_RATE> osc3((const int16_t*)BANK_FM[0]);
// Master LFO for Repeater and Parameter Modulation
Oscil<2048, MOZZI_CONTROL_RATE> modLFO(SIN2048_DATA);
EventDelay repeaterDelay;

ADSR<MOZZI_CONTROL_RATE, MOZZI_AUDIO_RATE> envelope;
ResonantFilter<LOWPASS> lpf; 

// Smoothers (Anti-Zipper EWMA)
float smoothCutoff = 100.0f;
float smoothMorph = 0.0f;

// Glide / Portamento Frequencies
float currentFreq1 = 65.41f; // C2
float currentFreq2 = 65.41f;
float currentFreq3 = 65.41f;

// --- PSRAM CLOUD SETUP ---
#define CLOUD_BUFFER_SIZE 131072 
int16_t* psramCloudBuffer = NULL;
uint32_t cloudWriteHead = 0;

// Helper: Quantize the 0-24 pot values into actual musical scales
int getQuantizedPitch(int potValue, int scaleType) {
    if (scaleType == 0) return potValue; // Chromatic
    
    int octave = potValue / 7;
    int degree = potValue % 7;
    int semitone = 0;
    
    if (scaleType == 1) { // Minor (Aeolian)
        int minScale[] = {0, 2, 3, 5, 7, 8, 10};
        semitone = minScale[degree];
    } else if (scaleType == 2) { // Major (Ionian)
        int majScale[] = {0, 2, 4, 5, 7, 9, 11};
        semitone = majScale[degree];
    } else if (scaleType == 3) { // Phrygian
        int phrygScale[] = {0, 1, 3, 5, 7, 8, 10};
        semitone = phrygScale[degree];
    }
    
    return (octave * 12) + semitone;
}

void setupAudioEngine() {
    // Envelope initial safety settings
    envelope.setADLevels(255, 255);
    
    // Set fixed Trance Gate / Repeater speed (125ms = 16th notes at 120bpm)
    repeaterDelay.set(125);
    modLFO.setFreq(1.5f); // 1.5Hz sweep for the Mod LFO

    // Allocate PSRAM Delay Line
    psramCloudBuffer = (int16_t*)ps_malloc(CLOUD_BUFFER_SIZE * sizeof(int16_t));
    if (psramCloudBuffer != NULL) {
        memset(psramCloudBuffer, 0, CLOUD_BUFFER_SIZE * sizeof(int16_t));
    }
    
    startMozzi();
}

void updateControl() {
    // --- 1. Envelope Shapes ---
    switch(state.envShape) {
        case 0: // Pluck
            envelope.setAttackTime(10); envelope.setDecayTime(150);
            envelope.setSustainLevel(0); envelope.setReleaseTime(50);
            break;
        case 1: // Brass
            envelope.setAttackTime(80); envelope.setDecayTime(200);
            envelope.setSustainLevel(150); envelope.setReleaseTime(300);
            break;
        case 2: // Pad
            envelope.setAttackTime(800); envelope.setDecayTime(400);
            envelope.setSustainLevel(200); envelope.setReleaseTime(1500);
            break;
    }

    // --- 2. Triggers (Manual & Repeater) ---
    if (state.triggerStab) {
        envelope.noteOn();
        state.triggerStab = false; 
    }
    
    if (state.lfoRepeater && !state.droneMode) {
        if (repeaterDelay.ready()) {
            envelope.noteOn();
            repeaterDelay.start();
        }
    }
    envelope.update();

    // --- 3. LFO Modulation Engine ---
    float lfoVal = modLFO.next() / 128.0f; // Scale to roughly -1.0 to 1.0
    float lfoCutoffMod = (state.lfoTarget == 1) ? lfoVal * 40.0f : 0.0f;
    float lfoMorphMod  = (state.lfoTarget == 2) ? lfoVal * 2.0f : 0.0f;
    float lfoPitchMod  = (state.lfoTarget == 3) ? lfoVal * 1.5f : 0.0f; // +/- 1.5 semitones

    // --- 4. Pitch & Glide Processing ---
    float glideAmount = 0.0f;
    if (state.glideSpeed == 1) glideAmount = 0.92f; // Medium Glide
    if (state.glideSpeed == 2) glideAmount = 0.99f; // Sluggish Pad Glide

    // Quantize the pot readings based on the selected scale
    int qPitch1 = getQuantizedPitch(state.osc1Pitch, state.activeScale);
    int qPitch2 = getQuantizedPitch(state.osc2Pitch, state.activeScale);
    int qPitch3 = getQuantizedPitch(state.osc3Pitch, state.activeScale);

    // Apply "Oh Sh*t" Sub-Bass Drop
    if (state.subBassDrop) qPitch1 -= 24;

    // Calculate Target Frequencies (Root + Quantized + LFO Pitch Mod)
    float targetFreq1 = mtof(state.rootNote + qPitch1 + lfoPitchMod);
    float targetFreq2 = mtof(state.rootNote + qPitch2 + lfoPitchMod);
    float targetFreq3 = mtof(state.rootNote + qPitch3 + lfoPitchMod);

    // EWMA Portamento Glide
    currentFreq1 = (currentFreq1 * glideAmount) + (targetFreq1 * (1.0f - glideAmount));
    currentFreq2 = (currentFreq2 * glideAmount) + (targetFreq2 * (1.0f - glideAmount));
    currentFreq3 = (currentFreq3 * glideAmount) + (targetFreq3 * (1.0f - glideAmount));

    osc1.setFreq(currentFreq1);
    osc2.setFreq(currentFreq2);
    osc3.setFreq(currentFreq3);

    // --- 5. Wavetable Bank & Morph Swapping ---
    float targetMorph = state.wavetableMorph + lfoMorphMod;
    smoothMorph = (smoothMorph * 0.9f) + (targetMorph * 0.1f);
    
    int currentFrame = (int)smoothMorph;
    if (currentFrame > 7) currentFrame = 7;
    if (currentFrame < 0) currentFrame = 0;

    const int16_t* targetTable; 
    switch (state.activeBank) {
        case 0:  targetTable = (const int16_t*)BANK_ANALOG[currentFrame]; break;
        case 1:  targetTable = (const int16_t*)BANK_GROWL[currentFrame]; break;
        case 2:  targetTable = (const int16_t*)BANK_FM[currentFrame]; break;
        default: targetTable = (const int16_t*)BANK_ANALOG[currentFrame]; break;
    }

    osc1.setTable(targetTable);
    osc2.setTable(targetTable);
    osc3.setTable(targetTable);

    // --- 6. Filter Smoothing ---
    float targetCutoff = map(state.filterCutoff, 0, 4095, 20, 210);
    smoothCutoff = (smoothCutoff * 0.92f) + (targetCutoff * 0.08f);
    
    // SAFEGUARD 2: Add 0.5f to round the float, preventing truncation micro-jitter
    int finalCutoff = (int)(smoothCutoff + lfoCutoffMod + 0.5f);
    if (finalCutoff > 230) finalCutoff = 230; 
    if (finalCutoff < 5) finalCutoff = 5;

    uint8_t res = map(state.filterRes, 0, 4095, 0, 200); 

    // SAFEGUARD 3: Only update the filter if the value actually changed!
    // This stops the 256Hz control loop from bleeding into the audio path.
    static int lastCutoff = -1;
    static uint8_t lastRes = -1;
    
    if (finalCutoff != lastCutoff || res != lastRes) {
        lpf.setCutoffFreqAndResonance((uint8_t)finalCutoff, res);
        lastCutoff = finalCutoff;
        lastRes = res;
    }
}

AudioOutput updateAudio() {
    // 1. Sum and scale down to prevent clipping
    float rawMix = (float)osc1.next() + (float)osc2.next() + (float)osc3.next();
    int safeMix = (int)(rawMix * 0.3333f); 
    
    // 2. Filter
    int filtered = lpf.next(safeMix); 
    
    // 3. VCA Routing
    int vcaOutput = 0;
    if (state.droneMode) {
        vcaOutput = filtered; 
    } else {
        // Multiply by 8-bit envelope (0-255) and shift down
        vcaOutput = (filtered * envelope.next()) >> 8; 
    }
    
    int finalOut = vcaOutput;

    // 4. PSRAM Ambient Wash
    if (psramCloudBuffer != NULL) {
        int tap1 = psramCloudBuffer[(cloudWriteHead - 16384 + CLOUD_BUFFER_SIZE) % CLOUD_BUFFER_SIZE];
        int tap2 = psramCloudBuffer[(cloudWriteHead - 40000 + CLOUD_BUFFER_SIZE) % CLOUD_BUFFER_SIZE];
        int tap3 = psramCloudBuffer[(cloudWriteHead - 85000 + CLOUD_BUFFER_SIZE) % CLOUD_BUFFER_SIZE];
        int tap4 = psramCloudBuffer[(cloudWriteHead - 131000 + CLOUD_BUFFER_SIZE) % CLOUD_BUFFER_SIZE];

        int cloudOut = (tap1 + tap2 + tap3 + tap4) >> 2;
        float washLevel = state.washMix / 4095.0f;
        int32_t feedbackSample = 0;

        // "Oh Sh*t" Modifier: Wash Freeze Loop
        if (state.washFreeze) {
            // Cut the dry feed, crank the feedback to nearly 100% (254/256)
            feedbackSample = ((tap4 * 254) >> 8); 
            finalOut = cloudOut; // Output only the frozen cloud
        } else {
            // Normal Wash Operation
            feedbackSample = (int32_t)(vcaOutput * washLevel) + ((tap4 * 190) >> 8); 
            finalOut = (int)((vcaOutput * (1.0f - washLevel)) + (cloudOut * washLevel));
        }

        // Hard integer clamp for safety
       if (feedbackSample > 32760) feedbackSample = 32760;     
        if (feedbackSample < -32760) feedbackSample = -32760;   

        psramCloudBuffer[cloudWriteHead] = (int16_t)feedbackSample;
        cloudWriteHead = (cloudWriteHead + 1) % CLOUD_BUFFER_SIZE;
    }

    // REMOVED THE BITSHIFT! Your audio is natively 16-bit now, so pass finalOut directly.
    return StereoOutput::from16Bit(finalOut, finalOut); 
}

void audioLoopWrapper() {
    audioHook();
}