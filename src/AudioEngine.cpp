#include "AudioEngine.h"
#include "GlobalState.h"

// --- MOZZI I2S & STEREO CONFIGURATION ---
// These MUST be defined BEFORE including MozziGuts.h
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_I2S_PIN_BCK 4
#define MOZZI_I2S_PIN_WS 5
#define MOZZI_I2S_PIN_DATA 6
#define MOZZI_CONTROL_RATE 128 
#define MOZZI_AUDIO_CHANNELS 2 // THIS enables StereoOutput::from16Bit

#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <Ead.h> 
#include <mozzi_midi.h> 

// --- FM ENGINE SETUP ---
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> op1(SIN2048_DATA); 
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> op2(SIN2048_DATA); 
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> op3(SIN2048_DATA); 
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> op4(SIN2048_DATA); 
Ead kEnvelope(AUDIO_RATE);

int mod_index_2 = 0, mod_index_3 = 0, mod_index_4 = 0;
uint8_t last_played_note = 0;

// --- GRANULAR MEMORY SETUP ---
const int PSRAM_BUFFER_SIZE = 65536; 
int16_t* granular_buffer;
volatile uint32_t write_head = 0;    

// --- GRAIN SPAWNER SETUP ---
const int MAX_GRAINS = 8;
const int WINDOW_SIZE = 1024;
uint8_t hann_window[WINDOW_SIZE];

struct Grain {
    bool active = false;
    float position = 0; 
    int length = 0;      
    int age = 0;         
};

Grain grain_pool[MAX_GRAINS];
int spawn_timer = 0;

void initWindowTable() {
    for (int i = 0; i < WINDOW_SIZE; i++) {
        hann_window[i] = (uint8_t)(127.5 * (1.0 - cos(2.0 * PI * i / (WINDOW_SIZE - 1))));
    }
}

void setupAudioEngine() {
    granular_buffer = (int16_t*) heap_caps_malloc(PSRAM_BUFFER_SIZE * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (granular_buffer != NULL) {
        memset(granular_buffer, 0, PSRAM_BUFFER_SIZE * sizeof(int16_t));
    }
    
    initWindowTable();
    startMozzi(MOZZI_CONTROL_RATE); 
}

void updateControl() {
    int timbre = globalState.fm_timbre; 
    int color = globalState.fm_color;   
    int env_shape = globalState.env_shape; 
    uint8_t current_note = globalState.active_note;
    uint8_t velocity = globalState.active_velocity;
    
    // --- FM CONTROL ---
    if (current_note != last_played_note && velocity > 0) {
        // FIXED: Expanded max decay to 4.5 seconds to prevent notes cutting off during drones
        unsigned int decay_ms = map(env_shape, 0, 4095, 50, 4500);
        kEnvelope.start(10, decay_ms); 
        last_played_note = current_note;
    }

    float base_freq = mtof(current_note);
    op1.setFreq(base_freq);
    
    int ratio_step = map(color, 0, 4095, 1, 5); 
    op2.setFreq(base_freq * (globalState.op2_ratio * ratio_step));
    op3.setFreq(base_freq * (globalState.op3_ratio * ratio_step)); 
    op4.setFreq(base_freq * globalState.op4_ratio);             

    float max_mod = globalState.max_mod_index;
    mod_index_2 = map(timbre, 0, 4095, 0, (int)max_mod); 
    mod_index_3 = map(timbre, 0, 4095, 0, (int)(max_mod * 0.5f));
    mod_index_4 = map(timbre, 0, 4095, 0, (int)(max_mod * 0.25f));

    // --- GRANULAR SPAWN CONTROL ---
    int density = globalState.gran_density; 
    int position = globalState.gran_position;

    int spawn_interval = map(density, 0, 4095, 64, 2); 

    if (++spawn_timer >= spawn_interval) {
        spawn_timer = 0;
        for (int i = 0; i < MAX_GRAINS; i++) {
            if (!grain_pool[i].active) {
                grain_pool[i].active = true;
                grain_pool[i].age = 0;
                
                grain_pool[i].length = map(density, 0, 4095, 8000, 1000); 
                int base_offset = map(position, 0, 4095, 2000, PSRAM_BUFFER_SIZE - 9000);
                int random_jitter = random(-500, 500);
                
                long start_pos = (write_head - base_offset + random_jitter + PSRAM_BUFFER_SIZE) % PSRAM_BUFFER_SIZE;
                grain_pool[i].position = (float)start_pos;
                break; 
            }
        }
    }
}

AudioOutput_t updateAudio() {
    // --- 1. COMPUTE FM ENGINE ---
    int env_level = kEnvelope.next(); 
    
    // THE DRONE LATCH: If Pot 3 is turned fully right, bypass the envelope!
    if (globalState.env_shape > 4000) {
        env_level = 255; // Infinite, uninterrupted sustain
    }

    long mod4 = (long)op4.next() * mod_index_4;
    long mod3 = (long)op3.phMod(mod4) * mod_index_3;
    long mod2 = (long)op2.phMod(mod3) * mod_index_2;
    long fm_out = (long)op1.phMod(mod2) * env_level; 

    // --- 2. RECORD TO PSRAM ---
    if (granular_buffer != NULL) {
        granular_buffer[write_head] = (int16_t)fm_out;
        write_head = (write_head + 1) % PSRAM_BUFFER_SIZE;
    }

    // --- 3. COMPUTE GRANULAR MULTIPLEXING ---
    long gran_out = 0; 
    
    for (int i = 0; i < MAX_GRAINS; i++) {
        if (grain_pool[i].active) {
            int buffer_idx = (int)grain_pool[i].position;
            long sample = granular_buffer[buffer_idx];

            int win_idx = (grain_pool[i].age * WINDOW_SIZE) / grain_pool[i].length;
            if (win_idx >= WINDOW_SIZE) win_idx = WINDOW_SIZE - 1; 
            
            long window_val = hann_window[win_idx]; 

            gran_out += (sample * window_val) >> 11;

            grain_pool[i].position += 1.0f; 
            if (grain_pool[i].position >= PSRAM_BUFFER_SIZE) grain_pool[i].position -= PSRAM_BUFFER_SIZE;
            
            grain_pool[i].age++;
            if (grain_pool[i].age >= grain_pool[i].length) {
                grain_pool[i].active = false;
            }
        }
    }

    // --- 4. ENGINE MIXER ---
    long mix_val = globalState.engine_mix; 
    long dry_level = 4095 - mix_val;
    long wet_level = mix_val;
    
    long final_out = ((fm_out * dry_level) + (gran_out * wet_level)) >> 12;

    if (final_out > 32767) final_out = 32767;
    if (final_out < -32768) final_out = -32768;

    return StereoOutput::from16Bit((int)final_out, (int)final_out); 
}

void audioTask(void *pvParameters) {
    setupAudioEngine();
    for (;;) {
        audioHook(); 
    }
}