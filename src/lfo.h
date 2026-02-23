#ifndef LFO_H
#define LFO_H

#include <stdint.h>
#include "wavetable.h"

#define NUM_LFOS 2

#define LFO_DST_AMP  0x01
#define LFO_DST_FREQ 0x02

typedef struct {
    enum oscillator_type_e type;
    uint32_t phase_pos;
    uint32_t tune;
    short cur_amplitude;
    short depth;
    uint8_t  dst;
} lfo_t;

extern lfo_t lfos[NUM_LFOS];

void lfo_init(void);

/// Tick the LFO by the given number of ticks. Increments the phase position
/// and stores the new amplitude.
static inline void lfo_tick(lfo_t * lfo, size_t ticks)
{
    lfo->phase_pos += (ticks * lfo->tune);

    switch (lfo->type)
    {
        case SINE:
            lfo->cur_amplitude = wavetable_get_amplitude(lfo->phase_pos, wavetable_get(SINE));
            break;
        case TRIANGLE:
            lfo->cur_amplitude = wavetable_triangle_component(lfo->phase_pos);
            break;
        case SQUARE:
            lfo->cur_amplitude = wavetable_square_component(lfo->phase_pos);
            break;
        case RAMP:
            lfo->cur_amplitude = wavetable_ramp_component(lfo->phase_pos);
            break;
        default:
            break;
    }
}

static inline void lfo_tick_all(size_t num_ticks)
{
    for (size_t lfo_idx = 0; lfo_idx < NUM_LFOS; ++lfo_idx)
    {
        if (NONE != lfos[lfo_idx].type)
        {
            lfo_tick(&lfos[lfo_idx], 1);
        }
    }
}

static inline int16_t lfo_mod_gain(uint8_t base_gain)
{
    int16_t gain_mod = 0;
    for (size_t lfo_idx = 0; lfo_idx < NUM_LFOS; ++lfo_idx)
    {
        lfo_t * lfo = &lfos[lfo_idx];
        if (LFO_DST_AMP & lfo->dst)
        {
            gain_mod += (int8_t)(((int32_t)lfo->cur_amplitude * (int32_t)lfo->depth) >> 23);
        }
    }

    int16_t gain = base_gain + gain_mod;
    if (gain < 0)
    {
        gain = 0;
    }

    return gain;
}

#endif
