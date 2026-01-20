#include <stddef.h>
#include <stdint.h>

#include "wavetable.h"

#ifndef VOICE_H
#define VOICE_H

#define POLYPHONY_COUNT 8


enum envelope_state_e {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
    NUM_envelope_sTATES
};

struct envelope_s
{
    uint32_t attack_samples;
    uint32_t decay_samples;
    uint32_t sustain_level;
    uint32_t release_samples;
};

typedef struct
{
    short * osc_ptr;
    uint8_t note;
    uint32_t phase;
    uint32_t tune;
    enum envelope_state_e amp_env_state;
    uint32_t amp_level;
    uint32_t amp_env_rate;
    uint64_t timestamp;
} voice_t;

void voice_init(void);
voice_t * voice_get(size_t voice_idx);
voice_t * voice_find_next(void);
voice_t * voice_find_for_note_off(uint8_t note);
void voice_envelope_tick(voice_t * voice);
void voice_note_on(voice_t * voice, uint8_t note);
void voice_note_off(voice_t * voice);

#endif