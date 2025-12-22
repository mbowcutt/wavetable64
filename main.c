#include <libdragon.h>
#include <audio.h>
#include <stdio.h>

#include "sine_lut.h"


///
/// COMPILER CONSTANTS/DEFINITIONS
///
#define SAMPLE_RATE 44100
#define NUM_AUDIO_BUFFERS 4
#define DEFAULT_FREQUENCY 440u

#define DEBUG_AUDIO_BUFFER_STATS 0


///
/// GLOBAL VARIABLES
///
static short * mix_buffer = NULL;
static bool mix_buffer_full = false;
static size_t mix_buffer_len = 0;

static float mix_gain = 0.5f;

#if DEBUG_AUDIO_BUFFER_STATS
static unsigned int write_cnt = 0;
static unsigned int local_write_cnt = 0;
static unsigned int no_write_cnt = 0;
#endif


///
/// STATIC PROTOTYPES
///
static void write_ai_buffer(short * buffer, size_t num_samples,
                            unsigned int * wav_pos_int,
                            unsigned int const wav_step_int,
                            uint16_t * wav_pos_frac,
                            uint16_t const wav_step_frac);

static void calculate_wavetable_step(unsigned int const frequency,
                                     unsigned int * step_int,
                                     uint16_t * step_frac);

static short interpolate_delta(int16_t const y0,
                               int16_t const y1,
                               uint16_t const frac);

static void audio_buffer_run(unsigned int * wav_pos_int,
                             unsigned int const wav_step_int,
                             uint16_t * wav_pos_frac,
                             uint16_t const wav_step_frac);

static void graphics_draw(unsigned int frequency);

static void input_poll(unsigned int * frequency,
                       unsigned int * wav_step_int,
                       uint16_t * wav_step_frac);


/// 
/// FUNCTION DEFINITIONS
///
static void calculate_wavetable_step(unsigned int frequency,
                                     unsigned int * step_int,
                                     uint16_t * step_frac)
{
    *step_int = (frequency * SINE_LUT_SIZE) / SAMPLE_RATE;
    *step_frac = (*step_int * frequency * UINT16_MAX) / frequency;

    graphics_draw(frequency);
}

static short interpolate_delta(int16_t const y0,
                               int16_t const y1,
                               uint16_t const frac)
{
    // Max delta: (+/-)UINT16_MAX
    //        =>  (INT16_MAX - INT16_MIN) =  UINT16_MAX
    //            (INT16_MIN - INT16_MAX) = -UINT16_MAX
    // Max frac: UINT16_MAX
    //
    // Worst case: UINT16_MAX * (+/-)UINT16_MAX = (+/-)UINT32_MAX
    // delta * frac product casted to int64_t

    return (short) (((int64_t)((int32_t)(y1 - y0) * (int32_t)frac)) >> 16);
}

static void write_ai_buffer(short * buffer, size_t num_samples,
                            unsigned int * wav_pos_int,
                            unsigned int const wav_step_int,
                            uint16_t * wav_pos_frac,
                            uint16_t const wav_step_frac)
{
    if (buffer && (num_samples > 0))
    {
        for (size_t i = 0; i < (2 * num_samples); i+=2)
        {
            unsigned int const next_wav_pos = (*wav_pos_int + 1) % SINE_LUT_SIZE;
            short y0 = sine_lut[*wav_pos_int];
            short const y1 = sine_lut[next_wav_pos];
            y0 = mix_gain * (y0 + interpolate_delta(y0, y1, *wav_pos_frac));

            buffer[i] = y0;
            buffer[i+1] = y0;  

            if ((UINT16_MAX - *wav_pos_frac) <= wav_step_frac)
            {
                *wav_pos_int = next_wav_pos;
                *wav_pos_frac = wav_step_frac - (UINT16_MAX - *wav_pos_frac);
            }
            else
            {
                *wav_pos_frac += wav_step_frac;
            }

            *wav_pos_int += wav_step_int;
            if (*wav_pos_int >= SINE_LUT_SIZE)
            {
                *wav_pos_int -= SINE_LUT_SIZE;
            }
        }
    }
}

static void graphics_draw(unsigned int frequency)
{
    static char str_freq[64] = {0};
    snprintf(str_freq, 64, "Freq: %d Hz", frequency);

    static char str_gain[64] = {0};
    snprintf(str_gain, 64, "Gain: %2.1f", mix_gain);

#if DEBUG_AUDIO_BUFFER_STATS
    static char str_dbg_audio_buf_stats[64] = {0};
    snprintf(str_dbg_audio_buf_stats, 64, "Write cnt: %u\nLocal write cnt:%u\nNo write cnt: %u",
             write_cnt, local_write_cnt, no_write_cnt);
#endif

    display_context_t disp = display_get();
	graphics_fill_screen(disp, 0);
    graphics_draw_text(disp, 30, 10, "N64 Wavetable Synthesizer\t\t\t\t\tv0.1");
	graphics_draw_text(disp, 30, 20, "(c) 2025 Michael Bowcutt");
	graphics_draw_text(disp, 30, 50, "Wave: Sine");
    graphics_draw_text(disp, 30, 58, str_freq);
    graphics_draw_text(disp, 30, 66, str_gain);
#if DEBUG_AUDIO_BUFFER_STATS
    graphics_draw_text(disp, 30, 74, str_dbg_audio_buf_stats);
#endif
	display_show(disp);
}

/// TODO: Return handler function pointer
static void input_poll(unsigned int * frequency,
                       unsigned int * wav_step_int,
                       uint16_t * wav_step_frac)
{
    joypad_poll();

    joypad_buttons_t ckeys = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    if (ckeys.d_left)
    {
        *frequency -= 10;
        calculate_wavetable_step(*frequency, wav_step_int, wav_step_frac);
    }
    else if (ckeys.d_right)
    {
        *frequency += 10;
        calculate_wavetable_step(*frequency, wav_step_int, wav_step_frac);
    }
    else if (ckeys.d_up)
    {
        mix_gain += 0.1f;
        if (mix_gain > 1.0f)
        {
            mix_gain = 1.0f;
        }

        graphics_draw(*frequency);
    }
    else if (ckeys.d_down)
    {
        mix_gain -= 0.1f;
        if (mix_gain < 0.0f)
        {
            mix_gain = 0.0f;
        }
        graphics_draw(*frequency);
    }
}

static void audio_buffer_run(unsigned int * wav_pos_int,
                             unsigned int const wav_step_int,
                             uint16_t * wav_pos_frac,
                             uint16_t const wav_step_frac)
{
    if (audio_can_write()) {
#if DEBUG_AUDIO_BUFFER_STATS
        ++write_cnt;
#endif
        short * buffer = audio_write_begin();
        if (mix_buffer_full)
        {
            memcpy(buffer, mix_buffer, mix_buffer_len);
            mix_buffer_full = false;
        }
        else
        {
            write_ai_buffer(buffer, audio_get_buffer_length(),
                            wav_pos_int, wav_step_int,
                            wav_pos_frac, wav_step_frac);
        }
        audio_write_end();
    }
    else if (!mix_buffer_full)
    {
#if DEBUG_AUDIO_BUFFER_STATS
        ++local_write_cnt;
#endif
        write_ai_buffer(mix_buffer, audio_get_buffer_length(),
                        wav_pos_int, wav_step_int,
                        wav_pos_frac, wav_step_frac);
        mix_buffer_full = true;
    }
    else
    {
#if DEBUG_AUDIO_BUFFER_STATS
        ++no_write_cnt;
#endif
    }
        
}

int main(void) {
    unsigned int frequency  = DEFAULT_FREQUENCY;
    unsigned int wav_pos_int = 0;
    uint16_t wav_pos_frac = 0;
    unsigned int wav_step_int = 0;
    uint16_t wav_step_frac = 0;

	joypad_init();
	debug_init_isviewer();
	debug_init_usblog();

	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);

	audio_init(SAMPLE_RATE, NUM_AUDIO_BUFFERS);
    calculate_wavetable_step(frequency, &wav_step_int, &wav_step_frac);

    mix_buffer_len = 2 * audio_get_buffer_length() * sizeof(short);
    mix_buffer = malloc(mix_buffer_len);
    if (!mix_buffer)
    {
        return -1;
    }

    graphics_draw(frequency);

	while(1) {
        
        input_poll(&frequency, &wav_step_int, &wav_step_frac);

        audio_buffer_run(&wav_pos_int, wav_step_int, &wav_pos_frac, wav_step_frac);
    }

    if (mix_buffer) free(mix_buffer);

	return 0;
}
