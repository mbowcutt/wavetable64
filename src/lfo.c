#include "lfo.h"

lfo_t lfos[NUM_LFOS];

void lfo_init(void)
{
    for (size_t lfo_idx = 0; lfo_idx < NUM_LFOS; ++lfo_idx)
    {
        lfo_t * lfo = &lfos[lfo_idx];
        lfo->type = NONE;
        lfo->phase_pos = 0u;
        lfo->tune = 0u;
        lfo->cur_amplitude = 0;
        lfo->depth = 0u;
        lfo->dst = 0u;
    }
}
