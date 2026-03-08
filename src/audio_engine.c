#include "audio_engine.h"

#include "gui.h"
#include "init.h"
#include "lfo.h"
#include "voice.h"
#include "wavetable.h"

#include <libdragon.h>
#include <midi.h>

#include <stddef.h>
#include <stdbool.h>

#define NUM_AUDIO_BUFFERS 4

static void audio_engine_callback(short * buffer, size_t num_samples);
static void audio_engine_synthesize(short * buffer, size_t num_samples);
static inline int32_t get_next_sample(void);
static inline void tick_envelopes(size_t num_ticks);

int32_t peak = 0;

static uint8_t mix_gain_factor = 64;

void audio_engine_init(void)
{
    audio_init(SAMPLE_RATE, NUM_AUDIO_BUFFERS);
    gui_splash(ALLOC_MIX_BUF);

    audio_set_buffer_callback(audio_engine_callback);

    // Must kick-start audio callback with dummy write
    audio_write_silence();
}

static void audio_engine_callback(short * buffer, size_t num_samples)
{
    if (buffer && (num_samples > 0))
    {
        audio_engine_synthesize(buffer, num_samples);
    }
}

void audio_engine_synthesize(short * buffer, size_t num_samples)
{
    if (buffer && (num_samples > 0))
    {
        peak = 0;
        for (uint16_t i = 0; i < num_samples; ++i)
        {
            int32_t sample = get_next_sample();

            tick_envelopes(1);

            lfo_tick_all(1);

            if ((sample > 0) && (sample > peak))
            {
                peak = sample;
            }
            else if ((sample < 0 && ((-1 * sample) > peak)))
            {
                peak = (-1 * sample);
            }

            if (sample > INT16_MAX)
                sample = INT16_MAX;
            else if (sample < INT16_MIN)
                sample = INT16_MIN;

            // Write stereo samples
            // TODO: Add panning
            buffer[i * 2] = (int16_t)sample;
            buffer[(i * 2) + 1] = (int16_t)sample;
        }
    }
}

static inline int32_t get_next_sample(void)
{
    int32_t amplitude = 0;

    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        voice_t * voice = voice_get(voice_idx);

        for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
        {
            wavetable_t * wav = &oscillators[wav_idx];

            if ((IDLE != voice->amp_env_state[wav_idx].stage)
                && (NONE != wav->shape)
                && (wav->gain > 0))
            {
                short component = wavetable_get_amplitude(voice->phase,
                                                          wavetable_get(wav->shape));

                component = (short)(((int64_t)component * (int64_t)voice->amp_env_state[wav_idx].level) / UINT32_MAX);
                component = (component * wav->gain) / MIDI_MAX_DATA_BYTE;

                amplitude += component;
            }
        }

        // Increment phase
        voice->phase += lfo_mod_tune(voice->tune);
    }

    int16_t gain = lfo_mod_gain(mix_gain_factor);

    return (amplitude * gain / MIDI_MAX_DATA_BYTE);
}

static inline void tick_envelopes(size_t num_ticks)
{
    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        voice_t * voice = voice_get(voice_idx);
        for (size_t wav_idx = 0; wav_idx < NUM_OSCILLATORS; ++wav_idx)
        {
            if ((IDLE != voice->amp_env_state[wav_idx].stage)
                && (NONE != oscillators[wav_idx].shape))
            {
                envelope_tick(&voice->amp_env_state[wav_idx], oscillators[wav_idx].amp_env_idx, num_ticks);
            }
        }
    }
}

void audio_engine_set_gain(uint8_t data)
{
    mix_gain_factor = data;
}
