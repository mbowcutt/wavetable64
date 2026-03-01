#ifndef WAVETABLE_H
#define WAVETABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "envelope.h"

#define NUM_OSCILLATORS 2
#define WT_BIT_DEPTH 11 // 2048 table entries
#define WT_SIZE (1 << WT_BIT_DEPTH)
#define ACCUMULATOR_BITS 32
#define FRAC_BITS (ACCUMULATOR_BITS - WT_BIT_DEPTH)

/// Enum representing the oscillator waveforms.
enum oscillator_shape_e {
    SINE,
    TRIANGLE,
    SQUARE,
    RAMP,
    NUM_OSC_TYPES,

    NONE = NUM_OSC_TYPES
};

/// Struct representing a waveform or voice component.
/// Includes an oscillator type, amp envelope, and mix amount.
typedef struct {
    enum oscillator_shape_e shape;
    uint8_t amp_env_idx;
    uint8_t gain;
} wavetable_t;

extern wavetable_t oscillators[NUM_OSCILLATORS];
extern short * osc_wave_tables[NUM_OSC_TYPES];

void wavetable_init(void);

uint32_t wavetable_get_midi_tune(uint8_t const note);
uint32_t wavetable_get_freq_tune(float freq_hz);

short wavetable_triangle_component(uint32_t const phase);
short wavetable_square_component(uint32_t const phase);
short wavetable_ramp_component(uint32_t const phase);

/// Return a pointer to the given wavetable type.
static inline short * wavetable_get(enum oscillator_shape_e osc)
{
    return osc_wave_tables[osc];
}

/// Perform linear interpolation based on two samples and the fractional
/// element of the phase accumulator.
static inline short wavetable_interpolate(int16_t const y0,
                                          int16_t const y1,
                                          uint32_t const frac)
{

    return (short) (((int64_t)((int32_t)(y1 - y0) * (int32_t)frac)) >> FRAC_BITS);
}

/// Return the amplitude of a wave at a given phase.
/// Extracts the integer and fractional elements of the phase accumulator. The
/// integer is used to lookup the exact sample and the following sample from
/// the table, and the fraction is used to interpolate beween them.
static inline short wavetable_get_amplitude(uint32_t const component_phase, short * wave_table)
{
    uint16_t const phase_int = (uint16_t const)((component_phase) >> FRAC_BITS);
    uint32_t const phase_frac = (uint32_t const)(component_phase & ((1 << FRAC_BITS) - 1));

    short const y0 = wave_table[phase_int];
    short const y1 = wave_table[phase_int + 1];

    return y0 + wavetable_interpolate(y0, y1, phase_frac);
}

#endif
