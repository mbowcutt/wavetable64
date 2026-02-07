#ifndef WAVETABLE_H
#define WAVETABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "envelope.h"

#define NUM_WAVETABLES 4
#define WT_BIT_DEPTH 11 // 2048 table entries
#define WT_SIZE (1 << WT_BIT_DEPTH)
#define ACCUMULATOR_BITS 32
#define FRAC_BITS (ACCUMULATOR_BITS - WT_BIT_DEPTH)

enum oscillator_type_e {
    SINE,
    TRIANGLE,
    SQUARE,
    RAMP,
    NUM_OSCILLATORS,

    NONE = NUM_OSCILLATORS
};

typedef struct {
    enum oscillator_type_e osc;
    struct envelope_s amp_env;
    uint8_t amt;
} wavetable_t;

extern wavetable_t waveforms[NUM_WAVETABLES];
extern short * osc_wave_tables[NUM_OSCILLATORS];

void wavetable_init(void);

uint32_t wavetable_get_tune(uint8_t const note);

static inline short * wavetable_get(enum oscillator_type_e osc)
{
    return osc_wave_tables[osc];
}

static inline short wavetable_interpolate(int16_t const y0,
                                          int16_t const y1,
                                          uint32_t const frac)
{

    return (short) (((int64_t)((int32_t)(y1 - y0) * (int32_t)frac)) >> FRAC_BITS);
}

static inline short wavetable_get_amplitude(uint32_t const component_phase, short * wave_table)
{
    uint16_t const phase_int = (uint16_t const)((component_phase) >> FRAC_BITS);
    uint32_t const phase_frac = (uint32_t const)(component_phase & ((1 << FRAC_BITS) - 1));

    short const y0 = wave_table[phase_int];
    short const y1 = wave_table[phase_int + 1];

    return y0 + wavetable_interpolate(y0, y1, phase_frac);
}

#endif
