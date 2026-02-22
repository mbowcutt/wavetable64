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
    uint16_t level; // 14 bits
    uint8_t  dst;
} lfo_t;

extern lfo_t lfos[NUM_LFOS];

void lfo_init(void);

#endif
