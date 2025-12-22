#include <libdragon.h>
#include <audio.h>
#include <stdio.h>

#include "sine_lut.h"

#define SAMPLE_RATE 44100
#define NUM_AUDIO_BUFFERS 4
#define DEFAULT_FREQUENCY 440u

static float mix_gain = 0.5f;

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

void graphics_draw(unsigned int frequency);

static void calculate_wavetable_step(unsigned int frequency,
                                     unsigned int * step_int,
                                     uint16_t * step_frac)
{
    *step_int = (frequency * SINE_LUT_SIZE) / SAMPLE_RATE;
    *step_frac = (*step_int * frequency * UINT16_MAX) / frequency;
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

    return (short) (((int64_t)(y1 - y0) * (int64_t)frac) / UINT16_MAX); 
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
            unsigned int next_wav_pos = (*wav_pos_int + 1) % SINE_LUT_SIZE;
            short const y0 = sine_lut[*wav_pos_int];
            short const y1 = sine_lut[next_wav_pos];
            short const val = mix_gain * (y0 + interpolate_delta(y0, y1, *wav_pos_frac));

            buffer[i] = val;
            buffer[i+1] = val;  

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

static unsigned int write_cnt = 0;
static unsigned int no_write_cnt = 0;


void graphics_draw(unsigned int frequency)
{
    static char str_freq[64] = {0};
    snprintf(str_freq, sizeof(str_freq), "Freq: %d Hz", frequency);

    static char str_write_cnt[64] = {0};
    snprintf(str_write_cnt, sizeof(str_write_cnt), "Write cnt: %u\nNo write cnt: %u", write_cnt, no_write_cnt);

    display_context_t disp = display_get();
	graphics_fill_screen(disp, 0);
    graphics_draw_text(disp, 10, 10, "N64 Wavetable Synthesizer - v0.1");
	graphics_draw_text(disp, 30, 20, "By Michael!");
	graphics_draw_text(disp, 30, 50, "Wave: Sine");
    graphics_draw_text(disp, 30, 58, str_freq);
    graphics_draw_text(disp, 30, 66, str_write_cnt);
	display_show(disp);
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
	// dfs_init(DFS_DEFAULT_LOCATION);

	audio_init(SAMPLE_RATE, NUM_AUDIO_BUFFERS);
    calculate_wavetable_step(frequency, &wav_step_int, &wav_step_frac);

    graphics_draw(frequency);

	while(1) {
        joypad_poll();

		joypad_buttons_t ckeys = joypad_get_buttons_pressed(JOYPAD_PORT_1);
		if (ckeys.d_left)
        {
            frequency -= 10;
            calculate_wavetable_step(frequency, &wav_step_int, &wav_step_frac);

            graphics_draw(frequency);
        }
		else if (ckeys.d_right)
        {
            frequency += 10;
            calculate_wavetable_step(frequency, &wav_step_int, &wav_step_frac);

            graphics_draw(frequency);
        }
        else if (ckeys.d_up)
        {
            mix_gain += 0.1f;
            if (mix_gain > 1.0f)
            {
                mix_gain = 1.0f;
            }

            graphics_draw(frequency);
        }
        else if (ckeys.d_down)
        {
            mix_gain -= 0.1f;
            if (mix_gain < 0.0f)
            {
                mix_gain = 0.0f;
            }
            graphics_draw(frequency);
        }

        if (audio_can_write()) {
            ++write_cnt;
            short * buffer = audio_write_begin();
            write_ai_buffer(buffer, audio_get_buffer_length(),
                            &wav_pos_int, wav_step_int,
                            &wav_pos_frac, wav_step_frac);
            audio_write_end();
        }
        else
        {
            ++no_write_cnt;
        }
    }

	return 0;
}
