#ifndef OSCIL16_H_
#define OSCIL16_H_

#include "Arduino.h"
#include "MozziHeadersOnly.h"
#include "mozzi_fixmath.h"
#include "FixMath.h"
#include "mozzi_pgmspace.h"

#ifdef OSCIL_DITHER_PHASE
#include "mozzi_rand.h"
#endif

#define OSCIL_F_BITS 16
#define OSCIL_F_BITS_AS_MULTIPLIER 65536
#define OSCIL_PHMOD_BITS 16

template <uint16_t NUM_TABLE_CELLS, uint16_t UPDATE_RATE>
class Oscil16
{
public:
    // FIX 1: Constructors now correctly match the Oscil16 class name
    Oscil16(const int16_t * TABLE_NAME):table(TABLE_NAME) {}
    Oscil16() {}

    inline int16_t next()
    {
        incrementPhase();
        return readTable();
    }

    void setTable(const int16_t * TABLE_NAME)
    {
        table = TABLE_NAME;
    }

    void setPhase(unsigned int phase)
    {
        phase_fractional = (uint32_t)phase << OSCIL_F_BITS;
    }

    void setPhaseFractional(uint32_t phase)
    {
        phase_fractional = phase;
    }

    uint32_t getPhaseFractional()
    {
        return phase_fractional;
    }

    inline int16_t phMod(Q15n16 phmod_proportion)
    {
        incrementPhase();
        // Force the cast to a non-const pointer just for the read macro
        return FLASH_OR_RAM_READ<int16_t>((int16_t*)(table + (((phase_fractional+(phmod_proportion * NUM_TABLE_CELLS))>>OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1))));
    }

    // FIX 2: Fixed-math templates reverted to int8_t to prevent SFINAE substitution failures
    template <int8_t NI, int8_t NF, uint8_t RANGE>
    inline int16_t phMod(SFix<NI,NF,RANGE> phmod_proportion)
    {
        return phMod(SFix<15,16>(phmod_proportion).asRaw());
    }
  
    inline int16_t phMod(SFix<15,16> phmod_proportion)
    {
        return phMod(phmod_proportion.asRaw());
    }

    inline void setFreq(int frequency) {
        phase_increment_fractional = ((uint32_t)frequency) * ((OSCIL_F_BITS_AS_MULTIPLIER*NUM_TABLE_CELLS)/UPDATE_RATE);
    }

    inline void setFreq(float frequency)
    { 
        phase_increment_fractional = (uint32_t)((((float)NUM_TABLE_CELLS * frequency)/UPDATE_RATE) * OSCIL_F_BITS_AS_MULTIPLIER);
    }

    template <int8_t NI, int8_t NF, uint64_t RANGE>
    inline void setFreq(UFix<NI,NF,RANGE> frequency)
    {
        setFreq_Q16n16(UFix<16,16>(frequency).asRaw());
    }

    inline void setFreq_Q24n8(Q24n8 frequency)
    {
        if ((256UL*NUM_TABLE_CELLS) >= UPDATE_RATE) {
            phase_increment_fractional = ((uint32_t)frequency) * ((256UL*NUM_TABLE_CELLS)/UPDATE_RATE);
        } else {
            phase_increment_fractional = ((uint32_t)frequency) / (UPDATE_RATE/(256UL*NUM_TABLE_CELLS));
        }
    }

    template <uint64_t RANGE>
    inline void setFreq(UFix<24,8,RANGE> frequency)
    {
        setFreq_Q24n8(frequency.asRaw());
    }

    inline void setFreq_Q16n16(Q16n16 frequency)
    {
        if (NUM_TABLE_CELLS >= UPDATE_RATE) {
            phase_increment_fractional = ((uint32_t)frequency) * (NUM_TABLE_CELLS/UPDATE_RATE);
        } else {
            phase_increment_fractional = ((uint32_t)frequency) / (UPDATE_RATE/NUM_TABLE_CELLS);
        }
    }

    template <uint64_t RANGE>
    inline void setFreq(UFix<16,16,RANGE> frequency)
    {
        setFreq_Q16n16(frequency.asRaw());
    }

    template <int8_t NI, int8_t NF, uint64_t RANGE>
    inline void setFreq(SFix<NI,NF,RANGE> frequency)
    {
        setFreq_Q16n16(UFix<16,16>(frequency).asRaw());
    }

    inline int16_t atIndex(unsigned int index)
    {
        // Force the cast here as well
        return FLASH_OR_RAM_READ<int16_t>((int16_t*)(table + (index & (NUM_TABLE_CELLS - 1))));
    }

    inline uint32_t phaseIncFromFreq(int frequency)
    {
        return ((uint32_t)frequency) * ((OSCIL_F_BITS_AS_MULTIPLIER*NUM_TABLE_CELLS)/UPDATE_RATE);
    }

    inline void setPhaseInc(uint32_t phaseinc_fractional)
    {
        phase_increment_fractional = phaseinc_fractional;
    }

private:
    static const uint16_t ADJUST_FOR_NUM_TABLE_CELLS = (NUM_TABLE_CELLS<2048) ? 8 : 0;

    inline void incrementPhase()
    {
        phase_fractional += phase_increment_fractional;
    }

    inline int16_t readTable()
    {
#ifdef OSCIL_DITHER_PHASE
        // Update the dithered read
        return FLASH_OR_RAM_READ<int16_t>((int16_t*)(table + (((phase_fractional + ((int)(xorshift96()>>16))) >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1))));
#else
        // Update the standard read
        return FLASH_OR_RAM_READ<int16_t>((int16_t*)(table + ((phase_fractional >> OSCIL_F_BITS) & (NUM_TABLE_CELLS - 1))));
#endif
    }

    uint32_t phase_fractional;
    uint32_t phase_increment_fractional;
    const int16_t * table;
};

#endif /* OSCIL16_H_ */