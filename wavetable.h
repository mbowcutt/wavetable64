#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef WAVETABLE_H
#define WAVETABLE_H

enum oscillator_type_e {
    SINE,
    TRIANGLE,
    SQUARE,
    RAMP,
    NUM_OSCILLATORS
};

short * wavetable_get(enum oscillator_type_e osc);

bool wavetable_generate_all(void);

uint32_t wavetable_get_tune(uint8_t const note);

short wavetable_get_amplitude(uint32_t const component_phase, short * wave_table);

#endif
