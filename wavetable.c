#include "wavetable.h"

#include "audio_engine.h"
#include "display.h"
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


static short * generate_sine_lut(float * sum_squares);
static short * generate_square_lut(float target_rms, size_t num_harmonics);
static short * generate_triangle_lut(float target_rms, size_t num_harmonics);
static short * generate_ramp_lut(float target_rms, size_t num_harmonics);

static void rms_normalize(short * lut, float * temp_lut, float target_rms, float sum_squares);

static void generate_midi_freq_tbl(void);

// TODO: Support direct (not wavetable) synthesis
// static short triangle_component(uint32_t const phase);
// static short square_component(uint32_t const phase);
// static short ramp_component(uint32_t const phase);

static short * osc_wave_tables[NUM_OSCILLATORS];
static float midi_freq_lut[MIDI_NOTE_MAX];


short * wavetable_get(enum oscillator_type_e osc)
{
    return osc_wave_tables[osc];
}


bool generate_wave_tables(void)
{
    bool status = true;
    float sum_squares = 0;
    float target_rms = 0;

    generate_midi_freq_tbl();

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

uint32_t get_tune(uint8_t const note)
{
    return (uint32_t)((midi_freq_lut[note] * ((uint64_t)1 << ACCUMULATOR_BITS)) / SAMPLE_RATE);
}

short interpolate_delta(int16_t const y0,
                        int16_t const y1,
                        uint32_t const frac)
{

    return (short) (((int64_t)((int32_t)(y1 - y0) * (int32_t)frac)) >> FRAC_BITS);
}

short phase_to_amplitude(uint32_t const component_phase, short * wave_table)
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

static void generate_midi_freq_tbl(void)
{
    for (int idx = 0; idx < MIDI_NOTE_MAX; ++idx)
    {
        midi_freq_lut[idx] = 440.0f * powf(2.0f, ((float)idx - 69) / 12.0f);
    }
}
