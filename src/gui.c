#include "gui.h"

#include "init.h"

#include <libdragon.h>

#include <stddef.h>

// static char * get_osc_type_str(void);

enum menu_screen
{
    SCREEN_MAIN,
    SCREEN_FILE,
    SCREEN_DEBUG,
    SCREEN_SETTINGS
};

static void gui_init_colors(void);

static void gui_print_header(display_context_t disp);
static void gui_print_footer(display_context_t disp);
static void gui_print_menu(display_context_t disp);

static uint32_t color_red = 0;
static uint32_t color_green = 0;
static uint32_t color_blue = 0;
static uint32_t color_white = 0;
static uint32_t color_black = 0;
static uint32_t color_gray = 0;

static enum menu_screen current_screen = SCREEN_MAIN;

void gui_init(void)
{
    display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    gui_init_colors();
    gui_splash(INIT);
}

static void gui_init_colors(void)
{
    color_red = graphics_make_color(0xFF, 0, 0, 0xFF);
    color_green = graphics_make_color(0, 0xFF, 0, 0xFF);
    color_blue = graphics_make_color(0, 0, 0xFF, 0xFF);
    color_white = graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF);
    color_black = graphics_make_color(0, 0, 0, 0xFF);
    color_gray = graphics_make_color(0x44, 0x44, 0x44, 0xFF);
}

void gui_draw(void)
{
    display_context_t disp = display_get();
    graphics_fill_screen(disp, color_black);
	gui_print_header(disp);
    gui_print_footer(disp);
    gui_print_menu(disp);

	display_show(disp);
}

void gui_splash(enum init_state_e init_state)
{
    display_context_t disp = display_get();
    graphics_fill_screen(disp, color_black);
    gui_print_header(disp);
    gui_print_footer(disp);

    if (GEN_SINE == init_state)
    {
        graphics_draw_text(disp, 60, 76, "Generating sine wave...");
    }
    else if (GEN_SINE < init_state)
    {
        graphics_draw_text(disp, 60, 76, "Sine wave generated.");
    }

    if (GEN_SQUARE == init_state)
    {
        graphics_draw_text(disp, 60, 84, "Generating square wave...");
    }
    else if (GEN_SQUARE < init_state)
    {
        graphics_draw_text(disp, 60, 84, "Square wave generated.");
    }

    if (GEN_TRIANGLE == init_state)
    {
        graphics_draw_text(disp, 60, 92, "Generating triangle wave...");
    }
    else if (GEN_TRIANGLE < init_state)
    {
        graphics_draw_text(disp, 60, 92, "Triangle wave generated.");
    }

    if (GEN_RAMP == init_state)
    {
        graphics_draw_text(disp, 60, 100, "Generating ramp wave...");
    }
    else if (GEN_RAMP < init_state)
    {
        graphics_draw_text(disp, 60, 100, "Ramp wave generated.");
    }

    if (GEN_FREQ_TBL == init_state)
    {
        graphics_draw_text(disp, 60, 108, "Generating MIDI note frequency LUT...");
    }
    else if (GEN_FREQ_TBL < init_state)
    {
        graphics_draw_text(disp, 60, 108, "MIDI note frequency LUT generated.");
    }

    if (ALLOC_MIX_BUF == init_state)
    {
        graphics_draw_text(disp, 60, 116, "Allocating mix buffer...");
    }
    else if (ALLOC_MIX_BUF < init_state)
    {
        graphics_draw_text(disp, 60, 116, "Mix buffer allocated.");
    }

    if (INIT_AUDIO == init_state)
    {
        graphics_draw_text(disp, 60, 124, "Initializing audio subsystem...");
    }
    else if (INIT_AUDIO < init_state)
    {
        graphics_draw_text(disp, 60, 124, "Audio subsystem initialized.");
    }

    display_show(disp);
}

void gui_warn_clip(void)
{
    display_context_t disp = display_get();
    gui_print_header(disp);
    graphics_draw_text(disp, 30, 40, "CLIP DETECTED!");
    display_show(disp);
}

static void gui_print_header(display_context_t disp)
{
    graphics_draw_box(disp, 0, 0, 512, 18, color_blue);
    graphics_draw_text(disp, 214, 9, "wavtable64");
    graphics_draw_line(disp, 0, 18, 512, 18, color_white);
}

static void gui_print_footer(display_context_t disp)
{
    graphics_draw_box(disp, 0, 218, 512, 22, color_gray);
    graphics_draw_line(disp, 0, 218, 512, 218, color_white);
    graphics_draw_text(disp, 128, 222, "(c) 2026 Michael Bowcutt <mwbowcutt@gmail.com>");
}

static void gui_print_menu(display_context_t disp)
{
    graphics_draw_box(disp, 0, 19, 512, 11, color_gray);
    graphics_draw_line(disp, 0, 30, 512, 30, color_white);

    graphics_draw_text(disp, 28, 21, "<< L");
    graphics_draw_text(disp, 458, 21, "R >>");

    switch (current_screen)
    {
        case SCREEN_MAIN:
            graphics_draw_box(disp, 108, 19, 40, 11, color_red);
            break;
        case SCREEN_FILE:
            graphics_draw_box(disp, 180, 19, 40, 11, color_red);
            break;
        case SCREEN_DEBUG:
            graphics_draw_box(disp, 252, 19, 48, 11, color_red);
            break;
        case SCREEN_SETTINGS:
            graphics_draw_box(disp, 332, 19, 72, 11, color_red);
            break;
        default:
            break;
    }
    graphics_draw_text(disp, 112, 21, "MAIN\tFILE\tDEBUG\tSETTINGS");
}

void gui_screen_next(void)
{
    if (SCREEN_SETTINGS == current_screen)
    {
        current_screen = SCREEN_MAIN;
    }
    else
    {
        ++current_screen;
    }
}

void gui_screen_prev(void)
{
    if (SCREEN_MAIN == current_screen)
    {
        current_screen = SCREEN_SETTINGS;
    }
    else
    {
        --current_screen;
    }
}

// static char * get_osc_type_str(void)
// {
//     switch (osc_type)
//     {
//         case SINE:
//             return "Sine";
//         case TRIANGLE:
//             return "Triangle";
//         case SQUARE:
//             return "Square";
//         case RAMP:
//             return "Ramp";
//         case NUM_OSCILLATORS:
//         default:
//             return "Unknown";
//     }
// }