#include "Storage.h"
#include "GlobalState.h"
#include <SD.h>
#include <SPI.h>

const int SD_CS = 10; 

int16_t* activeBank1 = nullptr;
int16_t* activeBank2 = nullptr;
int16_t* activeBank3 = nullptr;
int16_t* loadBuffer = nullptr;


const size_t BANK_SIZE_BYTES = 1048576; 

bool initStorage() {
    activeBank1 = (int16_t*)ps_malloc(BANK_SIZE_BYTES);
    activeBank2 = (int16_t*)ps_malloc(BANK_SIZE_BYTES);
    activeBank3 = (int16_t*)ps_malloc(BANK_SIZE_BYTES);
    loadBuffer  = (int16_t*)ps_malloc(BANK_SIZE_BYTES);

    if (!activeBank1 || !activeBank2 || !activeBank3 || !loadBuffer) {
        Serial.println("[FATAL] PSRAM Allocation for Wavetables failed!");
        return false;
    }

    memset(activeBank1, 0, BANK_SIZE_BYTES);
    memset(activeBank2, 0, BANK_SIZE_BYTES);
    memset(activeBank3, 0, BANK_SIZE_BYTES);
    memset(loadBuffer, 0, BANK_SIZE_BYTES);

    if (!SD.begin(SD_CS)) {
        Serial.println("[FATAL] SD Card Mount Failed!");
        return false;
    }
    
    Serial.println("[SD] Storage and PSRAM Initialized successfully.");
    return true;
}

bool loadWavetableToBank(const char* filename, int targetBank) {
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.printf("[SD ERROR] Cannot open %s\n", filename);
        return false;
    }

    if (file.size() != BANK_SIZE_BYTES) {
        Serial.printf("[SD ERROR] %s size mismatch! Must be exactly 1MB.\n", filename);
        file.close();
        return false;
    }

    size_t bytesRead = file.read((uint8_t*)loadBuffer, BANK_SIZE_BYTES);
    file.close();

    if (bytesRead != BANK_SIZE_BYTES) {
        Serial.println("[SD ERROR] Failed to read full file into PSRAM.");
        return false;
    }


    int16_t* temp = loadBuffer;
    
    if (targetBank == 0) {
        loadBuffer = activeBank1;
        activeBank1 = temp;
    } else if (targetBank == 1) {
        loadBuffer = activeBank2;
        activeBank2 = temp;
    } else if (targetBank == 2) {
        loadBuffer = activeBank3;
        activeBank3 = temp;
    }

    Serial.printf("[SD] Successfully hot-swapped %s into Bank %d\n", filename, targetBank);
    return true;
}