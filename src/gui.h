#ifndef GUI_H
#define GUI_H

#include <stddef.h>

#include "init.h"

#include <display.h>

#define NUM_DISP_BUFFERS 3


void gui_init(void);
void gui_draw_screen(void);
void gui_splash(enum init_state_e init_state);
void gui_draw_level_meter(display_context_t disp);

void gui_screen_next(void);
void gui_screen_prev(void);

#endif
