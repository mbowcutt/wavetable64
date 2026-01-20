#include "audio_engine.h"

#include "display.h"
#include "init.h"
#include "voice.h"
#include "wavetable.h"

#include <libdragon.h>

#include <stddef.h>
#include <stdbool.h>

#define NUM_AUDIO_BUFFERS 4

static void audio_engine_write_buffer(short * buffer, size_t const num_samples);

static short * mix_buffer = NULL;
static bool mix_buffer_full = false;
static size_t mix_buffer_len = 0;

static float mix_gain = 0.5f;

void audio_engine_init(void)
{
    audio_init(SAMPLE_RATE, NUM_AUDIO_BUFFERS);

    draw_splash(ALLOC_MIX_BUF);
    mix_buffer_len = 2 * audio_get_buffer_length() * sizeof(short);
    mix_buffer = malloc(mix_buffer_len);
    if (!mix_buffer)
    {
        //return -1;
    }
}

void audio_engine_run(void)
{
    if (audio_can_write())
    {
        short * buffer = audio_write_begin();
        if (buffer)
        {
            if (mix_buffer_full)
            {
                memcpy(buffer, mix_buffer, mix_buffer_len);
                mix_buffer_full = false;
            }
            else
            {
                audio_engine_write_buffer(buffer, audio_get_buffer_length());
            }
        }
        audio_write_end();
    }
    else if (!mix_buffer_full)
    {
        audio_engine_write_buffer(mix_buffer, audio_get_buffer_length());
        mix_buffer_full = true;
    }
}

static void audio_engine_write_buffer(short * buffer, size_t const num_samples)
{
    if (buffer && (num_samples > 0))
    {
        for (size_t i = 0; i < (2 * num_samples); i+=2)
        {
            short amplitude = 0;

            for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
            {
                voice_t * voice = voice_get(voice_idx);

                if (IDLE == voice->amp_env_state)
                    continue;

                short component = wavetable_get_amplitude(voice->phase, 
                                                          wavetable_get(voice->osc));

                component = (short)(((int64_t)component * (int64_t)voice->amp_level) / UINT32_MAX);

                amplitude += (short)(mix_gain * component);

                // Increment phase
                voice->phase += voice->tune;
                voice_envelope_tick(voice);
            }

            // Write stereo samples
            buffer[i] = amplitude;
            buffer[i+1] = amplitude;
        }
    }
}