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
    uint32_t amp_env_rate[NUM_WAVETABLES];
    uint64_t timestamp;
} voice_t;

extern uint32_t env_sample_lut[MIDI_MAX_DATA_BYTE + 1];

void voice_init(void);

voice_t * voice_get(size_t voice_idx);
voice_t * voice_find_next(void);
voice_t * voice_find_for_note_off(uint8_t note);

void voice_envelope_tick(voice_t * voice);

void voice_envelope_set_attack(uint8_t value);
void voice_envelope_set_decay(uint8_t value);
void voice_envelope_set_sustain(uint8_t value);
void voice_envelope_set_release(uint8_t value);

void voice_note_on(voice_t * voice, uint8_t note);
void voice_note_off(voice_t * voice);

#endif