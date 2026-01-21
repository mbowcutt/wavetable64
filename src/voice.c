#include "voice.h"

#include "audio_engine.h"
#include "wavetable.h"

#include <n64sys.h>
#include <midi.h>

#include <math.h>
#include <stddef.h>
#include <stdint.h>

static voice_t voices[POLYPHONY_COUNT];

static struct envelope_s amp_env;

static uint32_t env_sample_lut[MIDI_MAX_DATA_BYTE + 1] = {0};

static void init_env_sample_lut(float t_min, float t_max);

void voice_init(void)
{
    amp_env.attack_samples = SAMPLE_RATE;  // 1 second attack
    amp_env.decay_samples = SAMPLE_RATE;   // 1 second decay
    amp_env.sustain_level = 0x7FFFFFFFu; // 50% sustain level
    amp_env.release_samples = SAMPLE_RATE;  // 1 second release

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
        float seconds = t_min * powf(t_max / t_min, (float)idx / 127.0f);
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
                    voice->amp_env_rate = (UINT32_MAX - amp_env.sustain_level) / amp_env.decay_samples;
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
    amp_env.attack_samples = env_sample_lut[value];
}

void voice_envelope_set_decay(uint8_t value)
{
    amp_env.decay_samples = env_sample_lut[value];
}

void voice_envelope_set_sustain(uint8_t value)
{
    if (0 == value)
    {
        amp_env.sustain_level = 0;
    }
    else if (127 == value)
    {
        amp_env.sustain_level = UINT32_MAX;
    }
    else
    {
        amp_env.sustain_level = (uint32_t)(UINT32_MAX * ((float)value / 127.0f));
    }
}

void voice_envelope_set_release(uint8_t value)
{
    amp_env.release_samples = env_sample_lut[value];
}

void voice_note_on(voice_t * voice, uint8_t note)
{
    voice->note = note;
    voice->tune = wavetable_get_tune(note);
    voice->amp_env_state = ATTACK;
    voice->amp_env_rate = (UINT32_MAX - voice->amp_level) / amp_env.attack_samples;
    voice->timestamp = get_ticks();
}

void voice_note_off(voice_t * voice)
{
    if (IDLE != voice->amp_env_state)
    {
        voice->amp_env_state = RELEASE;
        voice->amp_env_rate = (voice->amp_level - 0) / amp_env.release_samples;
    }
}

