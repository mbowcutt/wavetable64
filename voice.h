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
    enum oscillator_type_e osc;
    uint8_t note;
    uint32_t phase;
    uint32_t tune;
    enum envelope_state_e amp_env_state;
    uint32_t amp_level;
    uint32_t amp_env_rate;
    uint64_t timestamp;
} voice_t;

void init_voices(void);
voice_t * voice_get(size_t voice_idx);
voice_t * find_next_voice(void);
voice_t * find_voice_to_close(uint8_t note);
void envelope_tick(voice_t * voice);
void note_on(voice_t * voice, uint8_t note);
void note_off(voice_t * voice);

#endif