#include <stdint.h>
#include <stddef.h>

#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#define SAMPLE_RATE 44100

void audio_engine_init(void);
void audio_engine_run(void);

void audio_engine_set_gain(uint8_t data);


#endif