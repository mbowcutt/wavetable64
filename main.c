#include <libdragon.h>
#include <audio.h>
#include <console.h>
#include <midi.h>

#include <math.h>
#include <stdio.h>

///
/// COMPILER CONSTANTS/DEFINITIONS
///
#define SAMPLE_RATE 44100
#define WT_BIT_DEPTH 11 // 2048 table entries
#define WT_SIZE (1 << WT_BIT_DEPTH)
#define ACCUMULATOR_BITS 32
#define FRAC_BITS (ACCUMULATOR_BITS - WT_BIT_DEPTH)
#define NUM_AUDIO_BUFFERS 4
#define NUM_NOTES 128
#define POLYPHONY_COUNT 8

#define DEBUG_AUDIO_BUFFER_STATS 0
#define DEBUG_CONSOLE 0


///
/// TYPES/DEFINITIONS
///
enum init_state_e
{
    INIT,
    GEN_SINE,
    GEN_SQUARE,
    GEN_TRIANGLE,
    GEN_RAMP,
    GEN_FREQ_TBL,
    ALLOC_MIX_BUF,
    INIT_AUDIO,
};

enum oscillator_type_e {
    SINE,
    TRIANGLE,
    SQUARE,
    RAMP,
    NUM_OSCILLATORS
};

enum envelope_state_e {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
    NUM_envelope_sTATES
};

struct envelope_s
{
    uint32_t attack_samples;
    uint32_t decay_samples;
    uint32_t sustain_level;
    uint32_t release_samples;
};

typedef struct
{
    uint32_t phase;
    uint32_t tune;
    enum envelope_state_e amp_env_state;
    uint32_t amp_level;
    uint32_t amp_env_rate;
} voice_t;

///
/// GLOBAL VARIABLES
///
static short * mix_buffer = NULL;
static bool mix_buffer_full = false;
static size_t mix_buffer_len = 0;

static float mix_gain = 0.5f;

static short * osc_wave_tables[NUM_OSCILLATORS];

static float midi_freq_lut[NUM_NOTES];

static float target_rms = 0;

#if DEBUG_AUDIO_BUFFER_STATS
static unsigned int write_cnt = 0;
static unsigned int local_write_cnt = 0;
static unsigned int no_write_cnt = 0;
#endif

static const char* note_names[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static size_t midi_in_bytes = 0;
static uint32_t midi_rx_ctr = 0;
static uint8_t midi_in_buffer[31] = {0};

static enum oscillator_type_e osc_type = SINE;
static struct envelope_s amp_env;
static voice_t voices[POLYPHONY_COUNT];

///
/// STATIC PROTOTYPES
///
static void write_ai_buffer(short * buffer, size_t const num_samples,
                            short * wave_table);

static inline uint32_t get_tune(uint8_t const note);

static short interpolate_delta(int16_t const y0,
                               int16_t const y1,
                               uint32_t const frac);

static void audio_buffer_run(short * wave_table);

static void draw_splash(enum init_state_e init_state);
static void graphics_draw(void);

static char * get_osc_type_str(void);

static void envelope_tick(void);

static void rms_normalize(short * lut, float * temp_lut, float target_rms, float sum_squares);

static void generate_midi_freq_tbl(void);
static bool generate_wave_tables(void);
static short * generate_sine_lut(float * sum_squares);
static short * generate_square_lut(float target_rms, size_t num_harmonics);
static short * generate_triangle_lut(float target_rms, size_t num_harmonics);
static short * generate_ramp_lut(float target_rms, size_t num_harmonics);

static void init_voices(void);

// TODO: Support direct (not wavetable) synthesis
// static short triangle_component(uint32_t const phase);
// static short square_component(uint32_t const phase);
// static short ramp_component(uint32_t const phase);


///
/// FUNCTION DEFINITIONS
///
static void rms_normalize(short * lut, float * temp_lut, float target_rms, float sum_squares)
{
    float const rms = sqrtf(sum_squares / WT_SIZE);
    float const scale = target_rms / rms;

    printf("Target: %f, RMS: %f, scale: %f\n", target_rms, rms, scale);

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        lut[i] = (short)(INT16_MAX * temp_lut[i] * scale);
    }

    lut[WT_SIZE] = lut[0];
}

static short * generate_sine_lut(float * sum_squares)
{
    short * lut = malloc((WT_SIZE + 1) * sizeof(short));
    if (!lut) return NULL;

    float const phase_step = (2.0f * M_PI) / WT_SIZE;

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        float temp = sinf((float)i * phase_step);
        (*sum_squares) += temp * temp;

        lut[i] = (short)(INT16_MAX/2 * temp);
    }

    lut[WT_SIZE] = lut[0];

    return lut;
}

static short * generate_square_lut(float target_rms, size_t num_harmonics)
{
    short * lut = malloc((WT_SIZE + 1) * sizeof(short));
    if (!lut) return NULL;
    float * temp_lut = malloc(WT_SIZE * sizeof(float));
    if (!lut) return NULL;

    float const phase_step = (2.0f * M_PI) / WT_SIZE;

    float sum_squares = 0;

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        temp_lut[i] = 0;
        for (size_t h = 1; h <= num_harmonics; h+=2)
        {
            temp_lut[i] += sinf((float)i * phase_step * h) / h;
        }
        sum_squares += temp_lut[i] * temp_lut[i];
    }

    rms_normalize(lut, temp_lut, target_rms, sum_squares);
    free(temp_lut);

    return lut;
}

static short * generate_triangle_lut(float target_rms, size_t num_harmonics)
{
    short * lut = malloc((WT_SIZE + 1) * sizeof(short));
    if (!lut) return NULL;
    float * temp_lut = malloc(WT_SIZE * sizeof(float));
    if (!lut) return NULL;

    float const phase_step = (2.0f * M_PI) / WT_SIZE;

    float sum_squares = 0;

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        temp_lut[i] = 0;
        float sign = 1.0f;
        for (size_t h = 1; h <= num_harmonics; h+=2)
        {
            temp_lut[i] += (sign * sinf((float)i * phase_step * h) / (float)(h * h));
            sign *= -1.0f;
        }
        sum_squares += temp_lut[i] * temp_lut[i];
    }

    rms_normalize(lut, temp_lut, target_rms, sum_squares);
    free(temp_lut);

    return lut;
}

static short * generate_ramp_lut(float target_rms, size_t num_harmonics)
{
    short * lut = malloc((WT_SIZE + 1) * sizeof(short));
    if (!lut) return NULL;
    float * temp_lut = malloc(WT_SIZE * sizeof(float));
    if (!lut) return NULL;

    float const phase_step = (2.0f * M_PI) / WT_SIZE;

    float sum_squares = 0;

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        temp_lut[i] = 0;
        for (size_t h = 1; h <= num_harmonics; ++h)
        {
            temp_lut[i] += sinf((float)i * phase_step * h) / (float)h;
        }
        sum_squares += temp_lut[i] * temp_lut[i];
    }

    rms_normalize(lut, temp_lut, target_rms, sum_squares);
    free(temp_lut);

    return lut;
}

static inline uint32_t get_tune(uint8_t const note)
{
    return (uint32_t)((midi_freq_lut[note] * ((uint64_t)1 << ACCUMULATOR_BITS)) / SAMPLE_RATE);
}

static short interpolate_delta(int16_t const y0,
                               int16_t const y1,
                               uint32_t const frac)
{

    return (short) (((int64_t)((int32_t)(y1 - y0) * (int32_t)frac)) >> FRAC_BITS);
}

static short phase_to_amplitude(uint32_t const component_phase, short * wave_table)
{
    uint16_t const phase_int = (uint16_t const)((component_phase) >> FRAC_BITS);
    uint32_t const phase_frac = (uint32_t const)(component_phase & ((1 << FRAC_BITS) - 1));

    short const y0 = wave_table[phase_int];
    short const y1 = wave_table[phase_int + 1];

    return y0 + interpolate_delta(y0, y1, phase_frac);
}

// static short triangle_component(uint32_t const phase)
// {
//     uint32_t phase_temp = phase + 0x40000000;
//     phase_temp >>= 15;
//     if (phase_temp & 0x10000)
//     {
//         phase_temp = 0x1FFFF - phase_temp;
//     }
//     return (short)(phase_temp - 0x8000);
// }

// static short square_component(uint32_t const phase)
// {
//     if (phase & 0x80000000)
//     {
//         return INT16_MIN;
//     }
//     else
//     {
//         return INT16_MAX;
//     }
// }

// static short ramp_component(uint32_t const phase)
// {
//     return (short)((phase) >> 16);
// }


static void write_ai_buffer(short * buffer, size_t const num_samples,
                            short * wave_table)
{
    if (buffer && (num_samples > 0))
    {
        for (size_t i = 0; i < (2 * num_samples); i+=2)
        {
            short amplitude = 0;

            // For each voice:
            voice_t * voice = &voices[0];
            short component = phase_to_amplitude(voice->phase, wave_table);

            component = (short)(((int64_t)component * (int64_t)voice->amp_level) / UINT32_MAX);

            amplitude += (short)(mix_gain * component);

            // Write stereo samples
            buffer[i] = amplitude;
            buffer[i+1] = amplitude;

            // Increment phase
            voice->phase += voice->tune;
            envelope_tick();
        }
    }
}

static char * get_osc_type_str(void)
{
    switch (osc_type)
    {
        case SINE:
            return "Sine";
        case TRIANGLE:
            return "Triangle";
        case SQUARE:
            return "Square";
        case RAMP:
            return "Ramp";
        case NUM_OSCILLATORS:
        default:
            return "Unknown";
    }
}

static void draw_splash(enum init_state_e init_state)
{
    display_context_t disp = display_get();
    graphics_fill_screen(disp, 0);
    graphics_draw_text(disp, 30, 10, "N64 Wavetable Synthesizer\t\t\t\t\tv0.1");
    graphics_draw_text(disp, 30, 18, "(c) 2026 Michael Bowcutt");

    if (GEN_SINE == init_state)
    {
        graphics_draw_text(disp, 60, 76, "Generating sine wave...");
    }
    else if (GEN_SINE < init_state)
    {
        graphics_draw_text(disp, 60, 76, "Sine wave generated.");
    }

    if (GEN_SQUARE == init_state)
    {
        graphics_draw_text(disp, 60, 84, "Generating square wave...");
    }
    else if (GEN_SQUARE < init_state)
    {
        graphics_draw_text(disp, 60, 84, "Sqare wave generated.");
    }

    if (GEN_TRIANGLE == init_state)
    {
        graphics_draw_text(disp, 60, 92, "Generating triangle wave...");
    }
    else if (GEN_TRIANGLE < init_state)
    {
        graphics_draw_text(disp, 60, 92, "Triangle wave generated.");
    }

    if (GEN_RAMP == init_state)
    {
        graphics_draw_text(disp, 60, 100, "Generating ramp wave...");
    }
    else if (GEN_RAMP < init_state)
    {
        graphics_draw_text(disp, 60, 100, "Ramp wave generated.");
    }

    if (GEN_FREQ_TBL == init_state)
    {
        graphics_draw_text(disp, 60, 108, "Generating MIDI note frequency LUT...");
    }
    else if (GEN_FREQ_TBL < init_state)
    {
        graphics_draw_text(disp, 60, 108, "MIDI note frequency LUT generated.");
    }

    if (ALLOC_MIX_BUF == init_state)
    {
        graphics_draw_text(disp, 60, 116, "Allocating mix buffer...");
    }
    else if (ALLOC_MIX_BUF < init_state)
    {
        graphics_draw_text(disp, 60, 116, "Mix buffer allocated.");
    }

    if (INIT_AUDIO == init_state)
    {
        graphics_draw_text(disp, 60, 124, "Initializing audio subsystem...");
    }
    else if (INIT_AUDIO < init_state)
    {
        graphics_draw_text(disp, 60, 124, "Audio subsystem initialized.");
    }

    display_show(disp);
}

static void graphics_draw(void)
{
    static char str_osc[64] = {0};
    snprintf(str_osc, 64, "Oscillator: %s", get_osc_type_str());

    static char str_gain[64] = {0};
    snprintf(str_gain, 64, "Gain: %2.1f", mix_gain);

    static char str_midi_data[64];
    snprintf(str_midi_data, 64, "MIDI RX got %d bytes (%ld): ", midi_in_bytes, midi_rx_ctr);
    for (uint8_t idx = 0; (idx < midi_in_bytes) && (idx < 31); ++idx)
    {
        char tmp[6];
        snprintf(tmp, 6, "0x%02X ", midi_in_buffer[idx]);
        strcat(str_midi_data, tmp);
    }

#if DEBUG_AUDIO_BUFFER_STATS
    static char str_dbg_audio_buf_stats[64] = {0};
    snprintf(str_dbg_audio_buf_stats, 64, "Write cnt: %u\nLocal write cnt:%u\nNo write cnt: %u",
             write_cnt, local_write_cnt, no_write_cnt);
#endif

    display_context_t disp = display_get();
	graphics_fill_screen(disp, 0);
    graphics_draw_text(disp, 30, 10, "N64 Wavetable Synthesizer\t\t\t\t\tv0.1");
	graphics_draw_text(disp, 30, 18, "(c) 2026 Michael Bowcutt");
	graphics_draw_text(disp, 30, 50, str_osc);
    graphics_draw_text(disp, 30, 66, str_gain);
    graphics_draw_text(disp, 30, 74, str_midi_data);
#if DEBUG_AUDIO_BUFFER_STATS
    graphics_draw_text(disp, 30, 74, str_dbg_audio_buf_stats);
#endif

	display_show(disp);
}

static void envelope_tick(void)
{
    voice_t * voice = &voices[0];
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
                    // TODO: Implement decay rate
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
                voice->amp_level -= voice->amp_env_rate;
                if (amp_env.sustain_level >= voice->amp_level)
                {
                    voice->amp_level = amp_env.sustain_level;
                    voice->amp_env_state = SUSTAIN;
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
        case NUM_envelope_sTATES:
        default:
            break;
    }
}

static void gain_up(void)
{
    mix_gain += 0.1f;
    if (mix_gain > 1.0f)
    {
        mix_gain = 1.0f;
    }
}

static void gain_down(void)
{
    mix_gain -= 0.1f;
    if (mix_gain < 0.0f)
    {
        mix_gain = 0.0f;
    }
}

static void wave_next(void)
{
    if (osc_type == RAMP)
    {
        osc_type = SINE;
    }
    else
    {
        ++osc_type;
    }
}

static void wave_prev(void)
{
    if (osc_type == SINE)
    {
        osc_type = RAMP;
    }
    else
    {
        --osc_type;
    }
}

static void note_on(voice_t * voice, uint8_t note)
{
    voice->tune = get_tune(note);
    voice->amp_env_state = ATTACK;
    voice->amp_env_rate = (UINT32_MAX - voice->amp_level) / amp_env.attack_samples;
}

static void note_off(voice_t * voice)
{
    if (IDLE != voice->amp_env_state)
    {
        voice->amp_env_state = RELEASE;
        voice->amp_env_rate = (voice->amp_level - 0) / amp_env.release_samples;
    }
}

static void audio_buffer_run(short * wave_table)
{
    if (audio_can_write()) {
#if DEBUG_AUDIO_BUFFER_STATS
        ++write_cnt;
#endif
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
                write_ai_buffer(buffer, audio_get_buffer_length(),
                                wave_table);
            }
        }
        audio_write_end();
    }
    else if (!mix_buffer_full)
    {
#if DEBUG_AUDIO_BUFFER_STATS
        ++local_write_cnt;
#endif
        write_ai_buffer(mix_buffer, audio_get_buffer_length(),
                        wave_table);
        mix_buffer_full = true;
    }
    else
    {
#if DEBUG_AUDIO_BUFFER_STATS
        ++no_write_cnt;
#endif
    }
}

static bool generate_wave_tables(void)
{
    bool status = true;
    float sum_squares = 0;

    draw_splash(GEN_SINE);
    osc_wave_tables[SINE] = generate_sine_lut(&sum_squares);
    if (!osc_wave_tables[SINE])
    {
        status = false;
    }
    else
    {
        target_rms = 0.5f * sqrtf(sum_squares/WT_SIZE);
    }

    size_t const num_harmonics = 120;
    if (status)
    {
        draw_splash(GEN_SQUARE);
        osc_wave_tables[SQUARE] = generate_square_lut(target_rms, num_harmonics);
        if (!osc_wave_tables[SQUARE])
        {
            status = false;
        }
    }

    if (status)
    {
        draw_splash(GEN_TRIANGLE);
        osc_wave_tables[TRIANGLE] = generate_triangle_lut(target_rms, num_harmonics);
        if (!osc_wave_tables[TRIANGLE])
        {
            status = false;
        }
    }

    if (status)
    {
        draw_splash(GEN_RAMP);
        osc_wave_tables[RAMP] = generate_ramp_lut(target_rms, num_harmonics);
        if (!osc_wave_tables[RAMP])
        {
            status = false;
        }
    }

    return status;
}

static void generate_midi_freq_tbl(void)
{
    for (int idx = 0; idx < NUM_NOTES; ++idx)
    {
        midi_freq_lut[idx] = 440.0f * powf(2.0f, ((float)idx - 69) / 12.0f);
    }
}

static void handle_midi_input(size_t midi_in_bytes)
{
    static midi_parser_state midi_parser = {0};

    midi_msg msg_buf[sizeof(midi_in_buffer) / sizeof(midi_msg)] = {0};
    size_t num_msgs = midi_process_messages(&midi_parser, 
                                             midi_in_buffer, midi_in_bytes,
                                             msg_buf, sizeof(msg_buf));
    for (size_t msg_idx = 0; msg_idx < num_msgs; ++msg_idx)
    {
        midi_msg msg = msg_buf[msg_idx];

        if ((MIDI_NOTE_OFF == msg.status)
            || ((MIDI_NOTE_ON == msg.status) && (0 == msg.data[1])))
        {
            // TODO: Find voice to close
            voice_t * voice = &voices[0];
            note_off(voice);
        }
        else if (MIDI_NOTE_ON == msg.status)
        {
            // TODO: Find free voice
            // TODO: Handle velocity
            voice_t * voice = &voices[0];
            note_on(voice, msg.data[0]);
        }
    }
}

static void init_voices(void)
{
    amp_env.attack_samples = SAMPLE_RATE;  // 1 second attack
    amp_env.decay_samples = SAMPLE_RATE;   // 1 second decay
    amp_env.sustain_level = 0x7FFFFFFFu; // 50% sustain level
    amp_env.release_samples = SAMPLE_RATE;  // 1 second release

    for (size_t voice_idx = 0; voice_idx < POLYPHONY_COUNT; ++voice_idx)
    {
        voice_t * voice = &voices[voice_idx];
        voice->phase = 0u;
        voice->tune = 0u;
        voice->amp_env_state = IDLE;
        voice->amp_level = 0u;
        voice->amp_env_rate = 0u;
    }
}

int main(void)
{
	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);

#if DEBUG_CONSOLE
    console_init();
    console_set_render_mode(RENDER_MANUAL);
#endif

    draw_splash(INIT);

    joypad_init();
    debug_init_isviewer();
    debug_init_usblog();

    if (!generate_wave_tables())
    {
        return -1;
    }

    draw_splash(GEN_FREQ_TBL);
    generate_midi_freq_tbl();

    draw_splash(INIT_AUDIO);
	audio_init(SAMPLE_RATE, NUM_AUDIO_BUFFERS);
    init_voices();

    draw_splash(ALLOC_MIX_BUF);
    mix_buffer_len = 2 * audio_get_buffer_length() * sizeof(short);
    mix_buffer = malloc(mix_buffer_len);
    if (!mix_buffer)
    {
        return -1;
    }

    graphics_draw();

    bool update_graphics = false;

	while(1) {
        midi_in_bytes = midi_rx_poll(JOYPAD_PORT_1,
                                     midi_in_buffer,
                                     sizeof(midi_in_buffer));

        if (midi_in_bytes > 0)
        {
            ++midi_rx_ctr;
            handle_midi_input(midi_in_bytes);
            update_graphics = true;
        }

        if (update_graphics)
        {
            graphics_draw();
            update_graphics = false;
        }

        audio_buffer_run(osc_wave_tables[osc_type]);
    }

    if (mix_buffer) free(mix_buffer);

	return 0;
}
