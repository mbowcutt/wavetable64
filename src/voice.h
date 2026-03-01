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
    struct envelope_state_s amp_env_state[NUM_OSCILLATORS];
    uint64_t timestamp;
} voice_t;

extern voice_t voices[POLYPHONY_COUNT];

void voice_init(void);

voice_t * voice_find_next(void);
voice_t * voice_find_for_note_off(uint8_t note);

void voice_note_on(voice_t * voice, uint8_t note);
void voice_note_off(voice_t * voice);

static inline voice_t * voice_get(size_t voice_idx)
{
    return &voices[voice_idx];
}

#endif
