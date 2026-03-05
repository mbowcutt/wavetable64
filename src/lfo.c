#include "lfo.h"

lfo_t lfos[NUM_LFOS];

void lfo_init(void)
{
    for (size_t lfo_idx = 0; lfo_idx < NUM_LFOS; ++lfo_idx)
    {
        lfo_t * lfo = &lfos[lfo_idx];
        lfo->shape = NONE;
        lfo->phase_pos = 0u;
        lfo->rate = 0.0f;
        lfo->tune = 0u;
        lfo->cur_amplitude = 0;
        lfo->depth = 0;
        lfo->dst = 0u;
    }
}

void lfo_set_rate(size_t lfo_idx, float rate_hz)
{
    lfo_t * lfo = &lfos[lfo_idx];
    lfo->rate = rate_hz;
    lfo->tune = wavetable_get_freq_tune(rate_hz);
}
