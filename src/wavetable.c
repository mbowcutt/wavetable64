#include "wavetable.h"

#include "audio_engine.h"
#include "envelope.h"
#include "gui.h"
#include "init.h"

#include <libdragon.h>
#include <midi.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>

static void wavetable_generate_all(void);
static void wavetable_generate_sine(float * sum_squares);
static void wavetable_generate_square(float target_rms, size_t num_harmonics);
static void wavetable_generate_triangle(float target_rms, size_t num_harmonics);
static void wavetable_generate_ramp(float target_rms, size_t num_harmonics);

static void wavetable_normalize(short * lut,
                                float target_rms,
                                float sum_squares);

static inline short wavetable_interpolate(int16_t const y0,
                                          int16_t const y1,
                                          uint32_t const frac);

static void wavetable_generate_midi_freq_tbl(void);

// TODO: Support direct (not wavetable) synthesis
// static short triangle_component(uint32_t const phase);
// static short square_component(uint32_t const phase);
// static short ramp_component(uint32_t const phase);

static float midi_freq_lut[MIDI_MAX_DATA_BYTE + 1];

/// Arrays for oscillator lookup tables. One additional sample is added to the
/// end to simplify interpolation step, as we won't need to check for wrapping.
static short sine_tbl[WT_SIZE + 1];
static short square_tbl[WT_SIZE + 1];
static short triangle_tbl[WT_SIZE + 1];
static short ramp_tbl[WT_SIZE + 1];

/// Temporary array used in generating lookup tables. Type is floating-point
/// for greater accuracy pre-RMS normalization. Additional sample not needed.
static float temp_tbl[WT_SIZE];

/// Array of pointers to osillaor lookup tables.
short * osc_wave_tables[NUM_OSC_TYPES] =
{
    sine_tbl,
    square_tbl,
    triangle_tbl,
    ramp_tbl
};

/// Storage location for oscillators/voice components.
wavetable_t oscillators[NUM_OSCILLATORS];


/// Initialize wavetable components.
/// Generates all wavetables and the midi to frequency lookup table.
/// Initializes a single sine wave voice with 50% sustain level.
void wavetable_init(void)
{
    wavetable_generate_all();
    wavetable_generate_midi_freq_tbl();

    oscillators[0].osc = SINE;
    oscillators[0].gain = 127;
    oscillators[0].amp_env = &envelopes[0];

    oscillators[1].osc = NONE;
    oscillators[1].gain = 0;
    oscillators[1].amp_env = &envelopes[1];
}

/// Genarates all oscillator lookup tables and RMS normalizes them to the same
/// level of perceived loudness.
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

/// Generate a sine wave lookup table.
/// Returns the sum of squares, used to calculate RMS to normalize the other
/// wavetables so they sound consistently loud.
/// Target amplitude is -6 dbFS, or 1/2 the 16 bit wide sample space.
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

/// Generates a band-limited square wave lookup table.
/// Formula: Sum of odd harmonics (h) with amplitudes scaled by 1/h.
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

/// Generates a band-limited triangle wave lookup table.
/// Formula: Sum of odd harmonics (h) with amplitudes scaled by 1/h^2 
/// and alternating phase (sign flip).
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

/// Generates a band-limited ramp (sawtooth) wave lookup table.
/// Formula: Sum of all harmonics (h) with amplitudes scaled by 1/h.
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

/// Adjusts a given lookup table to match a target RMS, given the sum of
/// squares of the input table.
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


/// Get the tune or stride value for the given note.
/// This looks up the note frequency from midi_freq_lut and calculates the
/// step rate through the accumulator. The phase accumulator should increment
/// by this value between each sample.
/// One pass through every 32 bit value represents one complete cycle. The step
/// rate is calculated by multiplying the frequency by the total number of
/// accumulator values (UINT32_MAX + 1), and dividing by the sample rate.
uint32_t wavetable_get_midi_tune(uint8_t const note)
{
    return wavetable_get_freq_tune(midi_freq_lut[note]);
}

uint32_t wavetable_get_freq_tune(float freq_hz)
{
    return (uint32_t)((freq_hz * ((uint64_t)1 << ACCUMULATOR_BITS)) / SAMPLE_RATE);
}


short wavetable_triangle_component(uint32_t const phase)
{
    uint32_t phase_temp = phase + 0x40000000;
    phase_temp >>= 15;
    if (phase_temp & 0x10000)
    {
        phase_temp = 0x1FFFF - phase_temp;
    }
    return (short)(phase_temp - 0x8000);
}

short wavetable_square_component(uint32_t const phase)
{
    if (phase & 0x80000000)
    {
        return INT16_MIN;
    }
    else
    {
        return INT16_MAX;
    }
}

short wavetable_ramp_component(uint32_t const phase)
{
    return (short)((phase) >> 16);
}

/// Generate MIDI note to frequency lookup table.
/// Produces an array midi_freq_lut of type float, where the index is the MIDI
/// note number [0,127] and the value is the frequency in Hz.
///
/// The table is based off Middle A (440 Hz) being note 69. Each octave up or
/// down doubles or halves the frequency. Thus, the equation for each note's
/// frequency is given by 440 * 2^((note_idx - 69)/12)
static void wavetable_generate_midi_freq_tbl(void)
{
    for (int note_idx = 0; note_idx <= MIDI_MAX_DATA_BYTE; ++note_idx)
    {
        midi_freq_lut[note_idx] = 440.0f * powf(2.0f, ((float)note_idx - 69) / 12.0f);
    }
}
