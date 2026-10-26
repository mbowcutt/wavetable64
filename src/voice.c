#include "voice.h"

#include "audio_engine.h"
#include "envelope.h"
#include "input.h"
#include "wavetable.h"

#include <n64sys.h>
#include <midi64.h>

#include <stddef.h>
#include <stdint.h>

voice_t voices[POLYPHONY_COUNT];

static bool mono = false;

static voice_t * voice_find_next(void);
static voice_t * voice_find_for_note_off(uint8_t note);
static void voice_all_osc_off(voice_t * voice);

void voice_init(void)
{
    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        voice_t * voice = &voices[voice_idx];
        voice->note = 0u;
        voice->phase = 0u;
        voice->tune = 0u;
        voice->timestamp = 0u;

        for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
        {
            voice->amp_env_state[wav_idx].stage = IDLE;
            voice->amp_env_state[wav_idx].level = 0u;
            voice->amp_env_state[wav_idx].rate = 0u;
        }
    }
}

void voice_set_mono(bool enable)
{
    mono = enable;
}

void voice_note_on(uint8_t note, uint8_t vel)
{
    // TODO: Handle velocity
    (void) vel;

    voice_t * voice = mono ? &voices[0] : voice_find_next();
    voice->note = note;
    voice->tune = wavetable_get_midi_tune(note);
    for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
    {
        if (NONE != oscillators[wav_idx].shape)
        {
            voice->amp_env_state[wav_idx].stage = ATTACK;
            voice->amp_env_state[wav_idx].rate
                = (UINT32_MAX - voice->amp_env_state[wav_idx].level)
                    / envelope_get_trans_samples(oscillators[wav_idx].amp_env_idx, ATTACK);
        }
    }
    voice->timestamp = get_ticks();
    midi_note_on(input_get_basic_channel(), note, vel);
}

void voice_note_off(uint8_t note)
{
    // TODO: handle velocity

    voice_t * voice = mono ? &voices[0] : voice_find_for_note_off(note);
    if (voice && (voice->note == note))
    {
        voice_all_osc_off(voice);
    }

    midi_note_off(input_get_basic_channel(), note, MIDI_MAX_DATA_BYTE);
}

void voice_all_off(void)
{
    for (size_t idx = 0; idx < POLYPHONY_COUNT; ++idx)
    {
        voice_all_osc_off(&voices[idx]);
    }
}

void voice_all_kill(void)
{
    for (size_t idx = 0; idx < POLYPHONY_COUNT; ++idx)
    {
        for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
        {
            voices[idx].amp_env_state[wav_idx].stage = IDLE;
        }
    }
}

static voice_t * voice_find_next(void)
{
    voice_t * voice = NULL;

    size_t oldest_voice_idx = 0;
    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        // Log the oldest voice index
        if (voices[voice_idx].timestamp < voices[oldest_voice_idx].timestamp)
        {
            oldest_voice_idx = voice_idx;
        }

        // If any waveform is active & non-IDLE, do not select it.
        for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
        {
            if ((NONE != oscillators[wav_idx].shape)
                && (IDLE != voices[voice_idx].amp_env_state[wav_idx].stage))
            {
                break;
            }

            if ((NUM_OSCILLATORS - 1) == wav_idx)
            {
                voice = &voices[voice_idx];
            }
        }
    }

    // If no idle voice was found, steal the oldest voice.
    if (!voice)
    {
        voice = &voices[oldest_voice_idx];
    }

    return voice;
}

static voice_t * voice_find_for_note_off(uint8_t note)
{
    voice_t * voice = NULL;

    // Find the oldest voice that matches the note
    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        // If note matches and it's the first or oldest match
        if ((note == voices[voice_idx].note)
            && ((!voice) || (voices[voice_idx].timestamp < voice->timestamp)))
        {
            // If any of the active oscillators are not IDLE or RELEASE,
            // the note is active - select it.
            for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
            {
                if ((NONE != oscillators[wav_idx].shape)
                    && (IDLE != voices[voice_idx].amp_env_state[wav_idx].stage)
                    && (RELEASE != voices[voice_idx].amp_env_state[wav_idx].stage))
                {
                    voice = &voices[voice_idx];
                    break;
                }
            }
        }
    }

    return voice;
}

static void voice_all_osc_off(voice_t * voice)
{
    for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
    {
        if (NONE != oscillators[wav_idx].shape)
        {
            if (IDLE != voice->amp_env_state[wav_idx].stage)
            {
                voice->amp_env_state[wav_idx].stage = RELEASE;
                voice->amp_env_state[wav_idx].rate = (voice->amp_env_state[wav_idx].level - 0) / envelope_get_trans_samples(oscillators[wav_idx].amp_env_idx, RELEASE);
            }
        }
    }
}
