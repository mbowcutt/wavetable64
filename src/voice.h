#ifndef VOICE_H
#define VOICE_H

#include <stddef.h>
#include <stdint.h>

#include "envelope.h"
#include "wavetable.h"

#include <midi.h>

#define POLYPHONY_COUNT 8

typedef struct
{
    uint8_t note;
    uint32_t phase;
    uint32_t tune;
    enum envelope_state_e amp_env_state[NUM_WAVETABLES];
    uint32_t amp_level[NUM_WAVETABLES];
    uint64_t amp_env_rate[NUM_WAVETABLES];
    uint64_t timestamp;
} voice_t;

extern voice_t voices[POLYPHONY_COUNT];
extern uint64_t env_sample_lut[MIDI_MAX_NRPN_VAL + 1];

void voice_init(void);

voice_t * voice_find_next(void);
voice_t * voice_find_for_note_off(uint8_t note);

void voice_envelope_set_attack(uint8_t value);
void voice_envelope_set_decay(uint8_t value);
void voice_envelope_set_sustain(uint8_t value);
void voice_envelope_set_release(uint8_t value);

void voice_note_on(voice_t * voice, uint8_t note);
void voice_note_off(voice_t * voice);

static inline voice_t * voice_get(size_t voice_idx)
{
    return &voices[voice_idx];
}

static inline void voice_envelope_tick(voice_t * voice, size_t wav_idx, size_t ticks)
{
    switch (voice->amp_env_state[wav_idx])
    {
        case IDLE:
            break;
        case ATTACK:
            if (UINT32_MAX > voice->amp_level[wav_idx])
            {
                if ((UINT32_MAX - voice->amp_level[wav_idx]) <= (ticks * voice->amp_env_rate[wav_idx]))
                {
                    voice->amp_level[wav_idx] = UINT32_MAX;
                    voice->amp_env_state[wav_idx] = DECAY;
                    voice->amp_env_rate[wav_idx] = (UINT32_MAX - waveforms[wav_idx].amp_env.sustain_level) / env_sample_lut[waveforms[wav_idx].amp_env.decay];
                }
                else
                {
                    voice->amp_level[wav_idx] += (ticks * voice->amp_env_rate[wav_idx]);
                }
            }
            break;
        case DECAY:
            if (waveforms[wav_idx].amp_env.sustain_level < voice->amp_level[wav_idx])
            {
                if ((voice->amp_level[wav_idx] - waveforms[wav_idx].amp_env.sustain_level)
                    <= (ticks * voice->amp_env_rate[wav_idx]))
                {
                    voice->amp_level[wav_idx] = waveforms[wav_idx].amp_env.sustain_level;
                    voice->amp_env_state[wav_idx] = SUSTAIN;
                }
                else
                {
                    voice->amp_level[wav_idx] -= (ticks * voice->amp_env_rate[wav_idx]);
                }
            }
            break;
        case SUSTAIN:
            break;
        case RELEASE:
            if (0 < voice->amp_level[wav_idx])
            {
                if ((voice->amp_level[wav_idx]) <= (ticks * voice->amp_env_rate[wav_idx]))
                {
                    voice->amp_level[wav_idx] = 0;
                    voice->amp_env_state[wav_idx] = IDLE;
                }
                else
                {
                    voice->amp_level[wav_idx] -= (ticks * voice->amp_env_rate[wav_idx]);
                }
            }
            break;
        case NUM_ENVELOPE_STATES:
        default:
            break;
    }
}

#endif
