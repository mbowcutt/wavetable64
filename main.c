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
/// TYPES/DEFINITIONS
///
typedef void (*HandlerFunc)(uint32_t*, uint32_t*);


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
static void write_ai_buffer(short * buffer, size_t const num_samples,
                            short const * amp_lut, size_t const amp_lut_len,
                            uint32_t * phase,
                            uint32_t const tune);

static inline uint32_t get_tune(uint32_t const frequency);

static short interpolate_delta(int16_t const y0,
                               int16_t const y1,
                               uint16_t const frac);

static void audio_buffer_run(uint32_t * phase,
                             uint32_t const tune);

static void graphics_draw(uint32_t const frequency);

static HandlerFunc input_poll(void);


/// 
/// FUNCTION DEFINITIONS
///
static inline uint32_t get_tune(uint32_t const frequency)
{
    return (uint32_t)(((uint64_t)frequency << 32) / SAMPLE_RATE);
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

static void write_ai_buffer(short * buffer, size_t const num_samples,
                            short const * amp_lut, size_t const amp_lut_len,
                            uint32_t * phase,
                            uint32_t const tune)
{
    if (buffer && (num_samples > 0))
    {
        for (size_t i = 0; i < (2 * num_samples); i+=2)
        {
            uint16_t const phase_int = (uint16_t const)((*phase) >> 16);
            uint16_t const phase_frac = (uint16_t const)*phase;

            // Phase to amplitude lookup
            short y0 = amp_lut[phase_int];
            short const y1 = amp_lut[phase_int + 1];
            y0 = mix_gain * (y0 + interpolate_delta(y0, y1, phase_frac));

            // Write stereo samples
            buffer[i] = y0;
            buffer[i+1] = y0;  

            // Increment phase
            *phase += tune;
        }
    }
}

static void graphics_draw(uint32_t const frequency)
{
    static char str_freq[64] = {0};
    snprintf(str_freq, 64, "Freq: %lu Hz", frequency);

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

static void pitch_up(uint32_t * frequency,
                     uint32_t * tune)
{
    *frequency += 10;
    *tune = get_tune(*frequency);
    graphics_draw(*frequency);
}

static void pitch_down(uint32_t * frequency,
                       uint32_t * tune)
{
    *frequency -= 10;
    *tune = get_tune(*frequency);
}

static void gain_up(uint32_t * frequency,
                    uint32_t * tune)
{
    mix_gain += 0.1f;
    if (mix_gain > 1.0f)
    {
        mix_gain = 1.0f;
    }
}

static void gain_down(uint32_t * frequency,
                      uint32_t * tune)
{
    mix_gain -= 0.1f;
    if (mix_gain < 0.0f)
    {
        mix_gain = 0.0f;
    }
}

static HandlerFunc input_poll(void)
{
    HandlerFunc handler = NULL;

    joypad_poll();

    joypad_buttons_t ckeys = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    if (ckeys.d_left)
    {
        handler = pitch_down;
    }
    else if (ckeys.d_right)
    {
        handler = pitch_up;
    }
    else if (ckeys.d_up)
    {
        handler = gain_up;
    }
    else if (ckeys.d_down)
    {
        handler = gain_down;
    }

    return handler;
}

static void audio_buffer_run(uint32_t * phase,
                             uint32_t const tune)
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
                            sine_lut, UINT16_MAX,
                            phase, tune);
        }
        audio_write_end();
    }
    else if (!mix_buffer_full)
    {
#if DEBUG_AUDIO_BUFFER_STATS
        ++local_write_cnt;
#endif
        write_ai_buffer(mix_buffer, audio_get_buffer_length(),
                        sine_lut, UINT16_MAX,
                        phase, tune);
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
    uint32_t frequency  = DEFAULT_FREQUENCY;
    uint32_t phase = 0u;
    uint32_t tune = 0u;

	joypad_init();
	debug_init_isviewer();
	debug_init_usblog();

	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);

	audio_init(SAMPLE_RATE, NUM_AUDIO_BUFFERS);
    tune = get_tune(frequency);

    mix_buffer_len = 2 * audio_get_buffer_length() * sizeof(short);
    mix_buffer = malloc(mix_buffer_len);
    if (!mix_buffer)
    {
        return -1;
    }

    graphics_draw(frequency);

	while(1) {
        HandlerFunc handler = input_poll();
        if (handler)
        {
            handler(&frequency, &tune);
            graphics_draw(frequency);
        }

        audio_buffer_run(&phase, tune);
    }

    if (mix_buffer) free(mix_buffer);

	return 0;
}
