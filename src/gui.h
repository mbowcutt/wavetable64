#ifndef GUI_H
#define GUI_H

#include <stdbool.h>
#include <stddef.h>

#include "init.h"

#include <display.h>

#define NUM_DISP_BUFFERS 3

extern enum menu_screen current_screen;
extern uint8_t selected_wav_idx;
extern uint8_t selected_field_main;
extern uint8_t selected_subfield;
extern bool field_selected;

void gui_init(void);
void gui_draw_screen(void);
void gui_splash(enum init_state_e init_state);
void gui_draw_level_meter(display_context_t disp);

void gui_screen_next(void);
void gui_screen_prev(void);

void gui_select_right(void);
void gui_select_left(void);
void gui_select_up(void);
void gui_select_down(void);
void gui_select(void);
void gui_deselect(void);

#endif
