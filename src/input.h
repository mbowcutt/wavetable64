#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef INPUT_H
#define INPUT_H

#define ANALOG_STICK_MAX 85

void input_init(void);
bool input_poll_and_handle(void);

void input_set_basic_channel(uint8_t channel);
uint8_t input_get_basic_channel(void);

size_t input_get_midi_in_byte_count(void);
uint32_t input_get_midi_rx_ctr(void);

#endif
