#include <stddef.h>

#ifndef INIT_H
#define INIT_H

enum init_state_e
{
    INIT,
    GEN_SINE,
    GEN_SQUARE,
    GEN_TRIANGLE,
    GEN_RAMP,
    GEN_FREQ_TBL,
    ALLOC_MIX_BUF,
    INIT_AUDIO,
};

#endif
