#include "voice.h"

#include "audio_engine.h"
#include "wavetable.h"

#include <n64sys.h>
#include <midi.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>

static voice_t voices[POLYPHONY_COUNT];

struct envelope_s amp_env;

uint32_t env_sample_lut[MIDI_MAX_DATA_BYTE + 1] = {0};

static void init_env_sample_lut(float t_min, float t_max);

void voice_init(void)
{
    amp_env.attack = 1;
    amp_env.decay = 5;
    amp_env.sustain_level = UINT32_MAX / 2;
    amp_env.release = 127;

    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        voice_t * voice = &voices[voice_idx];
        voice->osc_ptr = wavetable_get(SINE);
        voice->note = 0u;
        voice->phase = 0u;
        voice->tune = 0u;
        voice->amp_env_state = IDLE;
        voice->amp_level = 0u;
        voice->amp_env_rate = 0u;
        voice->timestamp = 0u;
    }

    init_env_sample_lut(0.005f, 10.0f);
}

static void init_env_sample_lut(float t_min, float t_max)
{
    for (size_t idx = 0; idx <= MIDI_MAX_DATA_BYTE; ++idx)
    {
        float seconds = t_min * powf(t_max / t_min, (float)idx / (float)MIDI_MAX_DATA_BYTE);
        env_sample_lut[idx] = (uint32_t)(seconds * SAMPLE_RATE);
    }
}

voice_t * voice_get(size_t voice_idx)
{
    return &voices[voice_idx];
}

voice_t * voice_find_next(void)
{
    voice_t * voice = NULL;

    size_t oldest_voice_idx = 0;
    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        if (IDLE == voices[voice_idx].amp_env_state)
        {
            voice = &voices[voice_idx];
            break;
        }
        else if (voices[voice_idx].timestamp < voices[oldest_voice_idx].timestamp)
        {
            oldest_voice_idx = voice_idx;
        }
    }

    if (!voice)
    {
        voice = &voices[oldest_voice_idx];
    }

    return voice;
}

voice_t * voice_find_for_note_off(uint8_t note)
{
    voice_t * voice = NULL;

    // Find the oldest voice that matches the note
    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        if ((note == voices[voice_idx].note)
            && ((IDLE != voices[voice_idx].amp_env_state) && RELEASE != voices[voice_idx].amp_env_state)
            && ((!voice)|| (voices[voice_idx].timestamp < voice->timestamp)))
        {
            voice = &voices[voice_idx];
        }
    }

    return voice;
}

void voice_envelope_tick(voice_t * voice)
{
    switch (voice->amp_env_state)
    {
        case IDLE:
            break;
        case ATTACK:
            if (UINT32_MAX > voice->amp_level)
            {
                if ((UINT32_MAX - voice->amp_level) < voice->amp_env_rate)
                {
                    voice->amp_level = UINT32_MAX;
                    voice->amp_env_state = DECAY;
                    voice->amp_env_rate = (UINT32_MAX - amp_env.sustain_level) / env_sample_lut[amp_env.decay];
                }
                else
                {
                    voice->amp_level += voice->amp_env_rate;
                }
            }
            break;
        case DECAY:
            if (amp_env.sustain_level < voice->amp_level)
            {
                if ((amp_env.sustain_level >= voice->amp_level)
                    || voice->amp_env_rate > voice->amp_level)
                {
                    voice->amp_level = amp_env.sustain_level;
                    voice->amp_env_state = SUSTAIN;
                }
                else
                {
                    voice->amp_level -= voice->amp_env_rate;
                }
            }
            break;
        case SUSTAIN:
            break;
        case RELEASE:
            if (0 < voice->amp_level)
            {
                if ((voice->amp_level) < voice->amp_env_rate)
                {
                    voice->amp_level = 0;
                    voice->amp_env_state = IDLE;
                }
                else
                {
                    voice->amp_level -= voice->amp_env_rate;
                }
            }
            break;
        case NUM_ENVELOPE_STATES:
        default:
            break;
    }
}

void voice_envelope_set_attack(uint8_t value)
{
    amp_env.attack = value;
}

void voice_envelope_set_decay(uint8_t value)
{
    amp_env.decay = value;
}

void voice_envelope_set_sustain(uint8_t value)
{
    amp_env.sustain_level = ((uint64_t)value * UINT32_MAX) / MIDI_MAX_DATA_BYTE;
}

void voice_envelope_set_release(uint8_t value)
{
    amp_env.release = value;
}

void voice_note_on(voice_t * voice, uint8_t note)
{
    voice->note = note;
    voice->tune = wavetable_get_tune(note);
    voice->amp_env_state = ATTACK;
    voice->amp_env_rate = (UINT32_MAX - voice->amp_level) / env_sample_lut[amp_env.attack];
    voice->timestamp = get_ticks();
}

void voice_note_off(voice_t * voice)
{
    if (IDLE != voice->amp_env_state)
    {
        voice->amp_env_state = RELEASE;
        voice->amp_env_rate = (voice->amp_level - 0) / env_sample_lut[amp_env.release];
    }
}
