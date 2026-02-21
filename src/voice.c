#include "voice.h"

#include "audio_engine.h"
#include "wavetable.h"

#include <n64sys.h>
#include <midi.h>

#include <stddef.h>
#include <stdint.h>

voice_t voices[POLYPHONY_COUNT];

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



voice_t * voice_find_next(void)
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
            if ((NONE != oscillators[wav_idx].osc)
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

voice_t * voice_find_for_note_off(uint8_t note)
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
                if ((NONE != oscillators[wav_idx].osc)
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

void voice_envelope_set_attack(uint8_t value)
{
    oscillators[0].amp_env->attack = ((uint16_t)value << 7);
}

void voice_envelope_set_decay(uint8_t value)
{
    oscillators[0].amp_env->decay = ((uint16_t)value << 7);
}

void voice_envelope_set_sustain(uint8_t value)
{
    oscillators[0].amp_env->sustain_level = ((((uint64_t)value) << 7) * UINT32_MAX) / MIDI_MAX_NRPN_VAL;
}

void voice_envelope_set_release(uint8_t value)
{
    oscillators[0].amp_env->release = ((uint16_t)value << 7);
}

void voice_note_on(voice_t * voice, uint8_t note)
{
    voice->note = note;
    voice->tune = wavetable_get_tune(note);
    for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
    {
        if (NONE != oscillators[wav_idx].osc)
        {
            voice->amp_env_state[wav_idx].stage = ATTACK;
            voice->amp_env_state[wav_idx].rate
                = (UINT32_MAX - voice->amp_env_state[wav_idx].level)
                    / env_sample_lut[oscillators[wav_idx].amp_env->attack];
        }
    }
    voice->timestamp = get_ticks();
}

void voice_note_off(voice_t * voice)
{
    for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
    {
        if (NONE != oscillators[wav_idx].osc)
        {
            if (IDLE != voice->amp_env_state[wav_idx].stage)
            {
                voice->amp_env_state[wav_idx].stage = RELEASE;
                voice->amp_env_state[wav_idx].rate = (voice->amp_env_state[wav_idx].level - 0) / env_sample_lut[oscillators[wav_idx].amp_env->release];
            }
        }
    }
}
