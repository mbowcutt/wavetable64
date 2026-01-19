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

bool generate_wave_tables(void);

uint32_t get_tune(uint8_t const note);

short interpolate_delta(int16_t const y0,
                        int16_t const y1,
                        uint32_t const frac);

short phase_to_amplitude(uint32_t const component_phase, short * wave_table);

#endif
