#ifndef WAVETABLE_H
#define WAVETABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "envelope.h"

#define NUM_WAVETABLES 4

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

void wavetable_init(void);
short * wavetable_get(enum oscillator_type_e osc);
uint32_t wavetable_get_tune(uint8_t const note);
short wavetable_get_amplitude(uint32_t const component_phase, short * wave_table);

#endif
