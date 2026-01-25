#include "wavetable.h"

#include "audio_engine.h"
#include "gui.h"
#include "init.h"

#include <libdragon.h>
#include <midi.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>

#define WT_BIT_DEPTH 11 // 2048 table entries
#define WT_SIZE (1 << WT_BIT_DEPTH)
#define ACCUMULATOR_BITS 32
#define FRAC_BITS (ACCUMULATOR_BITS - WT_BIT_DEPTH)

static void wavetable_generate_all(void);
static void wavetable_generate_sine(float * sum_squares);
static void wavetable_generate_square(float target_rms, size_t num_harmonics);
static void wavetable_generate_triangle(float target_rms, size_t num_harmonics);
static void wavetable_generate_ramp(float target_rms, size_t num_harmonics);

static void wavetable_normalize(short * lut,
                                float target_rms,
                                float sum_squares);

static short wavetable_interpolate(int16_t const y0,
                                   int16_t const y1,
                                   uint32_t const frac);

static void wavetable_generate_midi_freq_tbl(void);

// TODO: Support direct (not wavetable) synthesis
// static short triangle_component(uint32_t const phase);
// static short square_component(uint32_t const phase);
// static short ramp_component(uint32_t const phase);

static float midi_freq_lut[MIDI_MAX_DATA_BYTE + 1];

static short sine_tbl[WT_SIZE + 1];
static short square_tbl[WT_SIZE + 1];
static short triangle_tbl[WT_SIZE + 1];
static short ramp_tbl[WT_SIZE + 1];
static float temp_tbl[WT_SIZE];

static short * osc_wave_tables[NUM_OSCILLATORS] =
{
    sine_tbl,
    square_tbl,
    triangle_tbl,
    ramp_tbl
};

enum oscillator_type_e cur_osc = SINE;


void wavetable_init(void)
{
    wavetable_generate_all();
    wavetable_generate_midi_freq_tbl();
}

short * wavetable_get(enum oscillator_type_e osc)
{
    return osc_wave_tables[osc];
}


static void wavetable_generate_all(void)
{
    float sum_squares = 0;

    gui_splash(GEN_SINE);
    wavetable_generate_sine(&sum_squares);
    float target_rms = 0.5f * sqrtf(sum_squares/WT_SIZE);

    size_t const num_harmonics = 120;

    gui_splash(GEN_SQUARE);
    wavetable_generate_square(target_rms, num_harmonics);

    gui_splash(GEN_TRIANGLE);
    wavetable_generate_triangle(target_rms, num_harmonics);

    gui_splash(GEN_RAMP);
    wavetable_generate_ramp(target_rms, num_harmonics);
}

static void wavetable_generate_sine(float * sum_squares)
{
    float const phase_step = (2.0f * M_PI) / WT_SIZE;

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        float temp = sinf((float)i * phase_step);
        (*sum_squares) += temp * temp;

        sine_tbl[i] = (short)(INT16_MAX/2 * temp);
    }

    sine_tbl[WT_SIZE] = sine_tbl[0];
}

static void wavetable_generate_square(float target_rms, size_t num_harmonics)
{
    float const phase_step = (2.0f * M_PI) / WT_SIZE;

    float sum_squares = 0;

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        temp_tbl[i] = 0;
        for (size_t h = 1; h <= num_harmonics; h+=2)
        {
            temp_tbl[i] += sinf((float)i * phase_step * h) / h;
        }
        sum_squares += temp_tbl[i] * temp_tbl[i];
    }

    wavetable_normalize(square_tbl, target_rms, sum_squares);
}

static void wavetable_generate_triangle(float target_rms, size_t num_harmonics)
{
    float const phase_step = (2.0f * M_PI) / WT_SIZE;

    float sum_squares = 0;

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        temp_tbl[i] = 0;
        float sign = 1.0f;
        for (size_t h = 1; h <= num_harmonics; h+=2)
        {
            temp_tbl[i] += (sign * sinf((float)i * phase_step * h) / (float)(h * h));
            sign *= -1.0f;
        }
        sum_squares += temp_tbl[i] * temp_tbl[i];
    }

    wavetable_normalize(triangle_tbl, target_rms, sum_squares);
}

static void wavetable_generate_ramp(float target_rms, size_t num_harmonics)
{
    float const phase_step = (2.0f * M_PI) / WT_SIZE;

    float sum_squares = 0;

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        temp_tbl[i] = 0;
        for (size_t h = 1; h <= num_harmonics; ++h)
        {
            temp_tbl[i] += sinf((float)i * phase_step * h) / (float)h;
        }
        sum_squares += temp_tbl[i] * temp_tbl[i];
    }

    wavetable_normalize(ramp_tbl, target_rms, sum_squares);
}

static void wavetable_normalize(short * lut, float target_rms, float sum_squares)
{
    float const rms = sqrtf(sum_squares / WT_SIZE);
    float const scale = target_rms / rms;

    printf("Target: %f, RMS: %f, scale: %f\n", target_rms, rms, scale);

    for (size_t i = 0; i < WT_SIZE; ++i)
    {
        lut[i] = (short)(INT16_MAX * temp_tbl[i] * scale);
    }

    lut[WT_SIZE] = lut[0];
}

uint32_t wavetable_get_tune(uint8_t const note)
{
    return (uint32_t)((midi_freq_lut[note] * ((uint64_t)1 << ACCUMULATOR_BITS)) / SAMPLE_RATE);
}

short wavetable_interpolate(int16_t const y0,
                            int16_t const y1,
                            uint32_t const frac)
{

    return (short) (((int64_t)((int32_t)(y1 - y0) * (int32_t)frac)) >> FRAC_BITS);
}

short wavetable_get_amplitude(uint32_t const component_phase, short * wave_table)
{
    uint16_t const phase_int = (uint16_t const)((component_phase) >> FRAC_BITS);
    uint32_t const phase_frac = (uint32_t const)(component_phase & ((1 << FRAC_BITS) - 1));

    short const y0 = wave_table[phase_int];
    short const y1 = wave_table[phase_int + 1];

    return y0 + wavetable_interpolate(y0, y1, phase_frac);
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

static void wavetable_generate_midi_freq_tbl(void)
{
    for (int idx = 0; idx <= MIDI_MAX_DATA_BYTE; ++idx)
    {
        midi_freq_lut[idx] = 440.0f * powf(2.0f, ((float)idx - 69) / 12.0f);
    }
}
