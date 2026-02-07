#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#define SAMPLE_RATE 44100

extern int32_t peak;

void audio_engine_init(void);

void audio_engine_set_gain(uint8_t data);


#endif