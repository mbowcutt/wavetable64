#include "envelope.h"

#include <math.h>
#include "audio_engine.h"

uint64_t env_sample_lut[MIDI_MAX_NRPN_VAL + 1] = {0};

static void init_env_sample_lut(float t_min, float t_max);

static void init_env_sample_lut(float t_min, float t_max)
{
    for (size_t idx = 0; idx <= MIDI_MAX_NRPN_VAL; ++idx)
    {
        float seconds = t_min * powf(t_max / t_min, (float)idx / (float)MIDI_MAX_NRPN_VAL);
        env_sample_lut[idx] = (uint64_t)(seconds * SAMPLE_RATE);
    }
}

void envelope_init(void)
{
    init_env_sample_lut(0.001f, 10.0f);
}