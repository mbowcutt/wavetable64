#include "audio_engine.h"

#include "gui.h"
#include "init.h"
#include "voice.h"
#include "wavetable.h"

#include <libdragon.h>
#include <midi.h>

#include <stddef.h>
#include <stdbool.h>

#define NUM_AUDIO_BUFFERS 4

static void audio_engine_write_buffer(short * buffer, size_t const num_samples);

static short * mix_buffer = NULL;
static size_t mix_buffer_len = 0;

static uint8_t mix_gain_factor = 64;

int32_t high_watermark = 0;

void audio_engine_init(void)
{
    audio_init(SAMPLE_RATE, NUM_AUDIO_BUFFERS);

    gui_splash(ALLOC_MIX_BUF);
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
            audio_engine_write_buffer(buffer, audio_get_buffer_length());
        }
        audio_write_end();
    }
}

static void audio_engine_write_buffer(short * buffer, size_t const num_samples)
{
    if (buffer && (num_samples > 0))
    {
        high_watermark = 0;

        for (size_t i = 0; i < (2 * num_samples); i+=2)
        {
            int32_t amplitude = 0;

            for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
            {
                voice_t * voice = voice_get(voice_idx);

                if (IDLE == voice->amp_env_state)
                    continue;

                short component = wavetable_get_amplitude(voice->phase,
                                                          voice->osc_ptr);

                component = (short)(((int64_t)component * (int64_t)voice->amp_level) / UINT32_MAX);

                amplitude += component;

                // Increment phase
                voice->phase += voice->tune;
                voice_envelope_tick(voice);
            }

            amplitude = (amplitude * mix_gain_factor / MIDI_MAX_DATA_BYTE);

            if ((amplitude > 0) && (amplitude > high_watermark))
            {
                high_watermark = amplitude;
            }
            else if ((amplitude < 0 && ((-1 * amplitude) > high_watermark)))
            {
                high_watermark = (-1 * amplitude);
            }

            // Write stereo samples
            buffer[i] = amplitude;
            buffer[i+1] = amplitude;
        }
    }
}

void audio_engine_set_gain(uint8_t data)
{
    mix_gain_factor = data;
}
