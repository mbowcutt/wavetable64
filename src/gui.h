#ifndef GUI_H
#define GUI_H

#include <stdbool.h>
#include <stddef.h>

#include "init.h"

#include <display.h>

#define NUM_DISP_BUFFERS 3

void gui_init(void);
void gui_draw_screen(void);
void gui_splash(enum init_state_e init_state);
void gui_draw_level_meter(display_context_t disp);

bool gui_recv_continuous_input(void);

void gui_screen_next(void);
void gui_screen_prev(void);

void gui_nav_right(void);
void gui_nav_left(void);
void gui_nav_up(void);
void gui_nav_down(void);
void gui_select(void);
void gui_deselect(void);

#endif
