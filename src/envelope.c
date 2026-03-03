#include "envelope.h"

#include <math.h>
#include "audio_engine.h"

uint64_t env_sample_lut[MIDI_MAX_NRPN_VAL + 1] = {0};
struct envelope_s envelopes[NUM_ENVELOPES];

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

    for (size_t env_idx = 0; env_idx < NUM_ENVELOPES; ++env_idx)
    {
        envelopes[env_idx].attack = MIDI_MAX_NRPN_VAL / 2;
        envelopes[env_idx].decay = MIDI_MAX_NRPN_VAL;
        envelopes[env_idx].sustain_level = UINT32_MAX / 2;
        envelopes[env_idx].release = MIDI_MAX_NRPN_VAL / 2;
    }
}

void envelope_set_attack(uint8_t idx, uint16_t data)
{
    envelopes[idx].attack = data;
}

void envelope_set_decay(uint8_t idx, uint16_t data)
{
    envelopes[idx].decay = data;
}

void envelope_set_sustain(uint8_t idx, uint32_t data)
{
    envelopes[idx].sustain_level = data;
}

void envelope_set_release(uint8_t idx, uint16_t data)
{
    envelopes[idx].release = data;
}

uint64_t envelope_get_trans_samples(uint8_t idx, enum envelope_stage_e stage)
{
    uint64_t num_samples = 0;
    switch (stage)
    {
        case ATTACK:
            num_samples = env_sample_lut[envelopes[idx].attack];
            break;
        case DECAY:
            num_samples = env_sample_lut[envelopes[idx].decay];
            break;
        case RELEASE:
            num_samples = env_sample_lut[envelopes[idx].release];
            break;
        default:
            break;
    }
    return num_samples;
}
