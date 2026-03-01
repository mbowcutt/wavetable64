#ifndef ENVELOPE_H
#define ENVELOPE_H

#include <midi.h>
#include <stdint.h>

#define NUM_ENVELOPES 3

/// Enum representing each envelope stage.
enum envelope_stage_e {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
    NUM_ENVELOPE_STAGES
};

/// Struct containing the envelope settings.
/// Sustain is represented as an absolute level between [0,UINT32_MAX].
/// Attack, decay, and release are represented as 14 bit numbers and can be set
/// between [0,MIDI_MAX_NRPN_VAL].
struct envelope_s
{
    uint16_t attack;
    uint16_t decay;
    uint32_t sustain_level;
    uint16_t release;
};

struct envelope_state_s
{
    enum envelope_stage_e stage;
    uint32_t level;
    uint64_t rate;
};

extern uint64_t env_sample_lut[MIDI_MAX_NRPN_VAL + 1];
extern struct envelope_s envelopes[NUM_ENVELOPES];

void envelope_init(void);
void envelope_set_attack(uint8_t idx, uint16_t data);
void envelope_set_decay(uint8_t idx, uint16_t data);
void envelope_set_sustain(uint8_t idx, uint32_t data);
void envelope_set_release(uint8_t idx, uint16_t data);

uint64_t envelope_get_trans_samples(uint8_t idx, enum envelope_stage_e stage);

static inline void envelope_tick(struct envelope_state_s * env_state, uint8_t idx, size_t ticks)
{
    switch (env_state->stage)
    {
        case IDLE:
            break;
        case ATTACK:
            if (UINT32_MAX > env_state->level)
            {
                if ((UINT32_MAX - env_state->level) <= (ticks * env_state->rate))
                {
                    env_state->level = UINT32_MAX;
                    env_state->stage = DECAY;
                    env_state->rate = (UINT32_MAX - envelopes[idx].sustain_level) / env_sample_lut[envelopes[idx].decay];
                }
                else
                {
                    env_state->level += (ticks * env_state->rate);
                }
            }
            break;
        case DECAY:
            if (envelopes[idx].sustain_level < env_state->level)
            {
                if ((env_state->level - envelopes[idx].sustain_level)
                    <= (ticks * env_state->rate))
                {
                    env_state->level = envelopes[idx].sustain_level;
                    env_state->stage = SUSTAIN;
                }
                else
                {
                    env_state->level -= (ticks * env_state->rate);
                }
            }
            break;
        case SUSTAIN:
            break;
        case RELEASE:
            if (0 < env_state->level)
            {
                if ((env_state->level) <= (ticks * env_state->rate))
                {
                    env_state->level = 0;
                    env_state->stage = IDLE;
                }
                else
                {
                    env_state->level -= (ticks * env_state->rate);
                }
            }
            break;
        case NUM_ENVELOPE_STAGES:
        default:
            break;
    }
}

#endif
