#include <stddef.h>
#include <stdbool.h>

#ifndef INPUT_H
#define INPUT_H

bool input_poll_and_handle(void);
void handle_midi_input(size_t midi_in_bytes);

#endif
