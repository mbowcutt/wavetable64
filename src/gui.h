#include <stddef.h>

#include "init.h"

#ifndef DISPLAY_H
#define DISPLAY_H

void gui_init(void);
void gui_draw(void);
void gui_splash(enum init_state_e init_state);
void gui_warn_clip(void);
void gui_draw_level_meter(void);

void gui_screen_next(void);
void gui_screen_prev(void);

#endif
