#include "AudioEngine.h"
#include "GlobalState.h"
#include "I2SOutput.h" 
#include "VoiceManager.h"
#include "FloatLFO.h"
#include "PsramAbyss.h"
#include "Storage.h"

const int I2S_BCK = 4;
const int I2S_WS = 5;
const int I2S_DATA = 6;
const float AUDIO_RATE = 44100.0f;
const int CONTROL_RATE_DIVIDER = 46; 

I2SOutput dac;

// Initialized with nullptrs; we will inject the active banks dynamically
VoiceManager synthVoices(nullptr, nullptr, 4096, 128, AUDIO_RATE);

// Global LFO 1 (The Metronome)
FloatLFO globalLFO1(AUDIO_RATE / CONTROL_RATE_DIVIDER); 
PsramAbyss theAbyss;

const int AUDIO_BUFFER_SIZE = 64;
int16_t i2sBuffer[AUDIO_BUFFER_SIZE * 2]; 

float currentGlobalLFO1 = 0.0f;

// --- Helper: Resolves UI Bank Index to PSRAM Pointers ---
int16_t* getBankPtr(int index) {
    if (index == 0) return activeBank1;
    if (index == 1) return activeBank2;
    if (index == 2) return activeBank3;
    return activeBank1; // Default fallback
}

void setupAudioEngine() {
    if (!initStorage()) Serial.println("FATAL: Storage/PSRAM Init Failed!");

    loadWavetableToBank("/ANALOG_F.BIN", 0);
    loadWavetableToBank("/BASIC_SH.BIN", 1);
    loadWavetableToBank("/FM_BRASS.BIN", 2);

    if (!theAbyss.init()) Serial.println("FATAL: Abyss Allocation Failed!");
    if (!dac.begin(I2S_BCK, I2S_WS, I2S_DATA, (uint32_t)AUDIO_RATE)) {
        Serial.println("FATAL: I2S DAC Initialization Failed!");
    }
}

void updateControl() {
    // --- 1. Empty the MIDI Queue ---
    MidiEvent ev;
    while(xQueueReceive(midiQueue, &ev, 0) == pdTRUE) {
        if (ev.type == 0x90) {
            synthVoices.noteOn(ev.note, ev.velocity);
        } else if (ev.type == 0x80) {
            synthVoices.noteOff(ev.note);
        }
    }

    // --- 2. Update Global Modulators ---
    globalLFO1.setRate(state.lfo1Rate.load());
    currentGlobalLFO1 = globalLFO1.process(state.lfo1Wave.load());

    // --- 3. Dynamic PSRAM Bank Routing ---
    int16_t* b1 = getBankPtr(state.osc1Bank.load());
    int16_t* b2 = getBankPtr(state.osc2Bank.load());
    if (b1 && b2) {
        synthVoices.setBanks(b1, b2);
    }
    
    // --- 4. Sync Envelopes to the VoiceManager ---
    synthVoices.setEnvelopes(
        state.ampEnv[0].load(), state.ampEnv[1].load(), state.ampEnv[2].load(), state.ampEnv[3].load(),
        state.modEnv1[0].load(), state.modEnv1[1].load(), state.modEnv1[2].load(), state.modEnv1[3].load(),
        state.modEnv2[0].load(), state.modEnv2[1].load(), state.modEnv2[2].load(), state.modEnv2[3].load()
    );
}

int16_t updateAudio() {
    if (!activeBank1) return 0; 
    float rawMix = synthVoices.process(currentGlobalLFO1);

    float abyssSendAmt = state.abyssSend.load();
    float wetAbyss = theAbyss.process(
        rawMix * abyssSendAmt, 
        state.fxMode.load(), 
        state.fxFreeze.load()
    );

    float finalMix = rawMix + wetAbyss;

    float safeMix = tanhf(finalMix);
    return (int16_t)(safeMix * 32760.0f);
}

void audioTask(void *pvParameters) {
    setupAudioEngine();
    
    int controlCounter = 0; 
    size_t bytesWritten;

    for(;;) {
        for(int i = 0; i < AUDIO_BUFFER_SIZE; i++) {

            if (controlCounter++ >= CONTROL_RATE_DIVIDER) {
                updateControl();
                controlCounter = 0;
            }

            int16_t out = updateAudio();
            
            i2sBuffer[i * 2] = out;    
            i2sBuffer[(i * 2) + 1] = out; 
        }

        i2s_channel_write(dac.getTxChan(), i2sBuffer, sizeof(i2sBuffer), &bytesWritten, portMAX_DELAY);
    }
}

void engineNoteOn(uint8_t note, uint8_t velocity) {
    MidiEvent ev = {0x90, note, velocity};
    xQueueSend(midiQueue, &ev, 0);
}

void engineNoteOff(uint8_t note, uint8_t velocity) {
    MidiEvent ev = {0x80, note, velocity};
    xQueueSend(midiQueue, &ev, 0);
}