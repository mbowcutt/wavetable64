#include <libdragon.h>
#include <audio.h>
#include <console.h>

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
#define DEFAULT_NOTE 69 // Middle A (440Hz)
#define NUM_NOTES 128

#define DEBUG_AUDIO_BUFFER_STATS 0
#define DEBUG_CONSOLE 0


///
/// TYPES/DEFINITIONS
///
enum InitState_e
{
    INIT,
    GEN_SINE,
    GEN_SQUARE,
    GEN_TRIANGLE,
    GEN_RAMP,
    GEN_FREQ_TBL,
    ALLOC_MIX_BUF,
    INIT_AUDIO,
    COMPLETE,
};

enum OscillatorType_e {
    SINE,
    TRIANGLE,
    SQUARE,
    RAMP,
    NUM_OSCILLATORS
};

enum EnvelopeState_e {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
    NUM_ENVELOPE_STATES
};

struct Envelope_s
{
    enum EnvelopeState_e state;
    uint32_t level;
    uint32_t rate;

    uint32_t attack_samples;
    uint32_t decay_samples;
    uint32_t sustain_level;
    uint32_t release_samples;
};

typedef void (*HandlerFunc)(uint8_t * note,
                            uint32_t * tune,
                            enum OscillatorType_e *  osc_type,
                            struct Envelope_s * envelope);


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


///
/// STATIC PROTOTYPES
///
static void write_ai_buffer(short * buffer, size_t const num_samples,
                            short * wave_table,
                            uint32_t * phase,
                            uint32_t const tune,
                            struct Envelope_s * envelope);

static inline uint32_t get_tune(uint8_t const note);

static short interpolate_delta(int16_t const y0,
                               int16_t const y1,
                               uint32_t const frac);

static void audio_buffer_run(uint32_t * phase,
                             uint32_t const tune,
                             short * wave_table,
                             struct Envelope_s * envelope);

static void draw_splash(enum InitState_e init_state);
static void graphics_draw(enum OscillatorType_e osc_type, uint8_t const note);

static char * get_osc_type_str(enum OscillatorType_e const osc_type);

static void envelope_tick(struct Envelope_s * envelope);

static HandlerFunc input_poll(void);

static void rms_normalize(short * lut, float * temp_lut, float target_rms, float sum_squares);

static void generate_midi_freq_tbl(void);
static bool generate_wave_tables(void);
static short * generate_sine_lut(float * sum_squares);
static short * generate_square_lut(float target_rms, size_t num_harmonics);
static short * generate_triangle_lut(float target_rms, size_t num_harmonics);
static short * generate_ramp_lut(float target_rms, size_t num_harmonics);

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

static short phase_to_amplitude(uint32_t const phase, short * wave_table)
{
    uint16_t const phase_int = (uint16_t const)((phase) >> FRAC_BITS);
    uint32_t const phase_frac = (uint32_t const)(phase & ((1 << FRAC_BITS) - 1));

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
                            short * wave_table,
                            uint32_t * phase,
                            uint32_t const tune,
                            struct Envelope_s * envelope)
{
    if (buffer && (num_samples > 0))
    {
        for (size_t i = 0; i < (2 * num_samples); i+=2)
        {
            short amplitude = 0;

            // For each voice:
            short component = phase_to_amplitude(*phase, wave_table);

            component = (short)(((int64_t)component * (int64_t)envelope->level) / UINT32_MAX);

            amplitude += (short)(mix_gain * component);

            // Write stereo samples
            buffer[i] = amplitude;
            buffer[i+1] = amplitude;

            // Increment phase
            *phase += tune;
            envelope_tick(envelope);
        }
    }
}

static char * get_osc_type_str(enum OscillatorType_e const osc_type)
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

static void draw_splash(enum InitState_e init_state)
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

    if (COMPLETE == init_state)
    {
        graphics_draw_text(disp, 60, 132, "Ready... Press Start");
    }

    display_show(disp);

    if (COMPLETE == init_state)
    {
        do {
            joypad_poll();
        } while (!joypad_get_buttons_pressed(JOYPAD_PORT_1).start);
    }
}


static void graphics_draw(enum OscillatorType_e osc_type,
                          uint8_t const note)
{
    static char str_osc[64] = {0};
    snprintf(str_osc, 64, "Oscillator: %s", get_osc_type_str(osc_type));

    static char str_freq[64] = {0};
    snprintf(str_freq, 64, "MIDI note: %s%d (%u) - %f Hz",
             note_names[note % 12],
             (note / 12) - 1,
             note, midi_freq_lut[note]);

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
	graphics_draw_text(disp, 30, 18, "(c) 2025 Michael Bowcutt");
	graphics_draw_text(disp, 30, 50, str_osc);
    graphics_draw_text(disp, 30, 58, str_freq);
    graphics_draw_text(disp, 30, 66, str_gain);
#if DEBUG_AUDIO_BUFFER_STATS
    graphics_draw_text(disp, 30, 74, str_dbg_audio_buf_stats);
#endif
	display_show(disp);
}

static void envelope_tick(struct Envelope_s * envelope)
{
    switch (envelope->state)
    {
        case IDLE:
            break;
        case ATTACK:
            if (UINT32_MAX > envelope->level)
            {
                if ((UINT32_MAX - envelope->level) < envelope->rate)
                {
                    envelope->level = UINT32_MAX;
                    envelope->state = DECAY;
                }
                else
                {
                    envelope->level += envelope->rate;
                }
            }
            break;
        case DECAY:
            if (envelope->sustain_level < envelope->level)
            {
                envelope->level -= envelope->rate;
                if (envelope->sustain_level >= envelope->level)
                {
                    envelope->level = envelope->sustain_level;
                    envelope->state = SUSTAIN;
                }
            }
            break;
        case SUSTAIN:
            break;
        case RELEASE:
            if (0 < envelope->level)
            {
                if ((envelope->level) < envelope->rate)
                {
                    envelope->level = 0;
                    envelope->state = IDLE;
                }
                else
                {
                    envelope->level -= envelope->rate;
                }
            }
            break;
        case NUM_ENVELOPE_STATES:
        default:
            break;
    }
}

static void pitch_up(uint8_t * note,
                     uint32_t * tune,
                     enum OscillatorType_e * osc_type,
                     struct Envelope_s * envelope)
{
    ++(*note);
    *tune = get_tune(*note);
}

static void pitch_down(uint8_t * note,
                       uint32_t * tune,
                       enum OscillatorType_e * osc_type,
                       struct Envelope_s * envelope)
{
    --(*note);
    *tune = get_tune(*note);
}

static void gain_up(uint8_t * note,
                    uint32_t * tune,
                    enum OscillatorType_e * osc_type,
                    struct Envelope_s * envelope)
{
    mix_gain += 0.1f;
    if (mix_gain > 1.0f)
    {
        mix_gain = 1.0f;
    }
}

static void gain_down(uint8_t * note,
                      uint32_t * tune,
                      enum OscillatorType_e * osc_type,
                      struct Envelope_s * envelope)
{
    mix_gain -= 0.1f;
    if (mix_gain < 0.0f)
    {
        mix_gain = 0.0f;
    }
}

static void wave_next(uint8_t * note,
                      uint32_t * tune,
                      enum OscillatorType_e * osc_type,
                      struct Envelope_s * envelope)
{
    if (*osc_type == RAMP)
    {
        *osc_type = SINE;
    }
    else
    {
        ++(*osc_type);
    }
}

static void wave_prev(uint8_t * note,
                      uint32_t * tune,
                      enum OscillatorType_e * osc_type,
                      struct Envelope_s * envelope)
{
    if (*osc_type == SINE)
    {
        *osc_type = RAMP;
    }
    else
    {
        --(*osc_type);
    }
}

static void note_on(uint8_t * note,
                    uint32_t * tune,
                    enum OscillatorType_e * osc_type,
                    struct Envelope_s * envelope)
{
    envelope->state = ATTACK;
    envelope->rate = (UINT32_MAX - envelope->level) / envelope->attack_samples;
}

static void note_off(uint8_t * note,
                     uint32_t * tune,
                     enum OscillatorType_e * osc_type,
                     struct Envelope_s * envelope)
{
    if (IDLE != envelope->state)
    {
        envelope->state = RELEASE;
        envelope->rate = (envelope->level - 0) / envelope->release_samples;
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
    else if (ckeys.r)
    {
        handler = wave_next;
    }
    else if (ckeys.l)
    {
        handler = wave_prev;
    }
    else if (ckeys.a)
    {
        handler = note_on;
    }
    else
    {
        ckeys = joypad_get_buttons_released(JOYPAD_PORT_1);
        if (ckeys.a)
        {
            handler = note_off;
        }
    }

    return handler;
}

static void audio_buffer_run(uint32_t * phase,
                             uint32_t const tune,
                             short * wave_table,
                             struct Envelope_s * envelope)
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
                                wave_table,
                                phase, tune,
                                envelope);
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
                        wave_table,
                        phase, tune,
                        envelope);
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

int main(void)
{
#if DEBUG_CONSOLE
    console_init();
    console_set_render_mode(RENDER_AUTOMATIC);
#endif

    uint8_t note = DEFAULT_NOTE;
    uint32_t phase = 0u;
    uint32_t tune = 0u;
    enum OscillatorType_e osc_type = SINE;

    struct Envelope_s envelope = {
        .level = 0u,
        .state = IDLE,
        .attack_samples = SAMPLE_RATE,  // 1 second attack
        .decay_samples = SAMPLE_RATE,   // 1 second decay
        .sustain_level = 0x7FFFFFFF, // 50% sustain level
        .release_samples = SAMPLE_RATE,  // 1 second release
    };


	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
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
    tune = get_tune(note);

    draw_splash(ALLOC_MIX_BUF);
    mix_buffer_len = 2 * audio_get_buffer_length() * sizeof(short);
    mix_buffer = malloc(mix_buffer_len);
    if (!mix_buffer)
    {
        return -1;
    }

    draw_splash(COMPLETE);
    graphics_draw(osc_type, note);

	while(1) {
        HandlerFunc handler = input_poll();
        if (handler)
        {
            handler(&note, &tune, &osc_type, &envelope);
            graphics_draw(osc_type, note);
        }

        audio_buffer_run(&phase, tune, osc_wave_tables[osc_type], &envelope);
    }

    if (mix_buffer) free(mix_buffer);

	return 0;
}
