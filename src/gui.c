#include "gui.h"

#include "init.h"
#include "audio_engine.h"
#include "voice.h"
#include "wavetable.h"

#include <libdragon.h>
#include <midi.h>
#include <rdpq.h>
#include <rdpq_rect.h>
#include <rdpq_text.h>

#include <stddef.h>

enum menu_screen
{
    SCREEN_MAIN,
    SCREEN_FILE,
    SCREEN_DEBUG,
    SCREEN_SETTINGS
};

static void gui_print_header(display_context_t disp);
static void gui_print_footer(display_context_t disp);
static void gui_print_menu(display_context_t disp);
static void gui_print_main(display_context_t disp);

static void gui_draw_osc_type(display_context_t disp,
                              uint8_t wav_idx,
                              int x_base, int y_base);
static void gui_draw_amp_env(display_context_t disp,
                             uint8_t wav_idx,
                             int x_base, int y_base);
static void gui_draw_amt_box(display_context_t disp,
                             uint8_t wav_idx,
                             int x_base, int y_base);

static char * get_osc_type_str(enum oscillator_type_e osc_type);


static color_t color_red = RGBA32(0xFF, 0, 0, 0xFF);
static color_t color_green = RGBA32(0, 0xFF, 0, 0xFF);
static color_t color_blue = RGBA32(0, 0, 0xFF, 0xFF);
static color_t color_yellow = RGBA32(0xFF, 0xFF, 0, 0xFF);
static color_t color_white = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
static color_t color_black = RGBA32(0, 0, 0, 0xFF);
static color_t color_gray = RGBA32(0x44, 0x44, 0x44, 0xFF);

static enum menu_screen current_screen = SCREEN_MAIN;
static rdpq_font_t * font;

void gui_init(void)
{
    display_init(RESOLUTION_512x240, DEPTH_16_BPP, NUM_DISP_BUFFERS, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();

    font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_text_register_font(1, font);

    gui_splash(INIT);
}


void gui_draw_screen(void)
{
    display_context_t disp = display_get();
    rdpq_attach(disp, NULL);
    rdpq_set_mode_fill(color_black);
    rdpq_fill_rectangle(0, 0, display_get_width(), display_get_height());

	gui_print_header(disp);
    gui_print_footer(disp);
    gui_print_menu(disp);

    switch (current_screen)
    {
        case SCREEN_MAIN:
            gui_print_main(disp);
            break;
        case SCREEN_FILE:
            break;
        case SCREEN_DEBUG:
            break;
        case SCREEN_SETTINGS:
            break;
        default:
            break;
    }

    gui_draw_level_meter(disp);

    rdpq_detach_show();
}

void gui_splash(enum init_state_e init_state)
{
    display_context_t disp = display_get();
    rdpq_attach_clear(disp, NULL);
    rdpq_set_mode_fill(color_black);
    rdpq_fill_rectangle(0, 0, display_get_width(), display_get_height());

    gui_print_header(disp);
    gui_print_footer(disp);

    if (GEN_SINE == init_state)
    {
        rdpq_text_print(NULL, 1, 60, 76, "Generating sine wave...");
    }
    else if (GEN_SINE < init_state)
    {
        rdpq_text_print(NULL, 1, 60, 76, "Sine wave generated.");
    }

    if (GEN_SQUARE == init_state)
    {
        rdpq_text_print(NULL, 1, 60, 84, "Generating square wave...");
    }
    else if (GEN_SQUARE < init_state)
    {
        rdpq_text_print(NULL, 1, 60, 84, "Square wave generated.");
    }

    if (GEN_TRIANGLE == init_state)
    {
        rdpq_text_print(NULL, 1, 60, 92, "Generating triangle wave...");
    }
    else if (GEN_TRIANGLE < init_state)
    {
        rdpq_text_print(NULL, 1, 60, 92, "Triangle wave generated.");
    }

    if (GEN_RAMP == init_state)
    {
        rdpq_text_print(NULL, 1, 60, 100, "Generating ramp wave...");
    }
    else if (GEN_RAMP < init_state)
    {
        rdpq_text_print(NULL, 1, 60, 100, "Ramp wave generated.");
    }

    if (GEN_FREQ_TBL == init_state)
    {
        rdpq_text_print(NULL, 1, 60, 108, "Generating MIDI note frequency LUT...");
    }
    else if (GEN_FREQ_TBL < init_state)
    {
        rdpq_text_print(NULL, 1, 60, 108, "MIDI note frequency LUT generated.");
    }

    if (ALLOC_MIX_BUF == init_state)
    {
        rdpq_text_print(NULL, 1, 60, 116, "Allocating mix buffer...");
    }
    else if (ALLOC_MIX_BUF < init_state)
    {
        rdpq_text_print(NULL, 1, 60, 116, "Mix buffer allocated.");
    }

    if (INIT_AUDIO == init_state)
    {
        rdpq_text_print(NULL, 1, 60, 124, "Initializing audio subsystem...");
    }
    else if (INIT_AUDIO < init_state)
    {
        rdpq_text_print(NULL, 1, 60, 124, "Audio subsystem initialized.");
    }

    rdpq_detach_show();
}

static void gui_print_header(display_context_t disp)
{
    rdpq_set_mode_fill(color_blue);
    rdpq_fill_rectangle(0, 0, 512, 18);
    rdpq_text_print(NULL, 1, 214, 16, "wavtable64");
    // graphics_draw_line(disp, 0, 18, 512, 18, graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF));
}

static void gui_print_footer(display_context_t disp)
{
    rdpq_set_mode_fill(color_gray);
    rdpq_fill_rectangle(0, 218, 512, 240);
    // graphics_draw_line(disp, 0, 218, 512, 218, graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF));
    rdpq_text_print(NULL, 1, 128, 230, "(c) 2026 Michael Bowcutt <mwbowcutt@gmail.com>");
}

static void gui_print_menu(display_context_t disp)
{
    rdpq_set_mode_fill(color_gray);
    rdpq_fill_rectangle(0, 19, 512, 30);
    // graphics_draw_line(disp, 0, 30, 512, 30, graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF));

    rdpq_set_fill_color(color_red);
    switch (current_screen)
    {
        case SCREEN_MAIN:
            rdpq_fill_rectangle(108, 19, 140, 30);
            break;
        case SCREEN_FILE:
            rdpq_fill_rectangle(166, 19, 198, 30);
            break;
        case SCREEN_DEBUG:
            rdpq_fill_rectangle(224, 19, 262, 30);
            break;
        case SCREEN_SETTINGS:
            rdpq_fill_rectangle(288, 19, 344, 30);
            break;
        default:
            break;
    }

    rdpq_text_print(NULL, 1, 28, 28, "<< L");
    rdpq_text_print(NULL, 1, 112, 28, "MAIN");
    rdpq_text_print(NULL, 1, 170, 28, "FILE");
    rdpq_text_print(NULL, 1, 228, 28, "DEBUG");
    rdpq_text_print(NULL, 1, 292, 28, "SETTINGS");
    rdpq_text_print(NULL, 1, 458, 28, "R >>");

}

void gui_draw_level_meter(display_context_t disp)
{
    bool detach_disp = false;
    if (NULL == disp)
    {
        disp = display_get();
        rdpq_attach(disp, NULL);
        detach_disp = true;
    }

    rdpq_set_mode_fill(RGBA32(0, 0, 0, 0xFF));
    rdpq_fill_rectangle(0, 208, 512, 218);

    rdpq_text_print(NULL, 1, 30, 216, "LEVEL:");

    size_t const total_boxes = 35;
    size_t const warn_level = total_boxes * 65 / 100;
    size_t const clip_level = total_boxes * 9 / 10;
    size_t const break_level = total_boxes * 11 / 10;

    size_t num_boxes = (high_watermark * total_boxes) / INT16_MAX;

    int x_pos = 80;
    rdpq_set_mode_fill(color_green);

    for (size_t idx = 0; idx < num_boxes; ++idx)
    {
        rdpq_fill_rectangle(x_pos, 208, x_pos + 8, 216);

        if (warn_level == idx)
        {
            rdpq_set_mode_fill(color_yellow);
        }
        else if (clip_level == idx)
        {
            rdpq_set_mode_fill(color_red);
        }
        else if (break_level == idx)
        {
            break;
        }

        x_pos += 10;
    }

    if (detach_disp)
    {
        rdpq_detach_show();
    }
}

static void gui_print_main(display_context_t disp)
{
    int x_base = 26;
    int y_base = 34;

    rdpq_text_print(NULL, 1, x_base, y_base + 9, "OSC:");
    rdpq_text_print(NULL, 1, x_base, y_base + 140 + 12 - 2, "AMP:");
    rdpq_text_print(NULL, 1, x_base, y_base + 140 + 12 + 2 + 8, "AMT:");

    x_base += (5 * 8);

    for (uint8_t wav_idx = 0; wav_idx < NUM_WAVETABLES; ++wav_idx)
    {
        int x_loc = x_base;
        int y_loc = y_base;
        gui_draw_osc_type(disp, wav_idx, x_loc, y_loc);

        y_loc += 10 + 2;

        gui_draw_amp_env(disp, wav_idx, x_loc, y_loc);

        y_loc += 140 + 2;

        gui_draw_amt_box(disp, wav_idx, x_loc, y_loc);

        x_base += (62 + 4);
    }
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

static uint8_t selected_wav_idx = 0;
static uint8_t selected_field_main = 0;

static void gui_draw_osc_type(display_context_t disp,
                              uint8_t wav_idx,
                              int x_base, int y_base)
{
    enum oscillator_type_e const osc_type = waveforms[wav_idx].osc;
    int const box_width = 62;

    if ((0 == selected_field_main) && (wav_idx == selected_wav_idx))
        rdpq_set_mode_fill(color_blue);
    else
        rdpq_set_mode_fill(color_gray);

    rdpq_fill_rectangle(x_base, y_base, x_base + box_width, y_base + 10);
    rdpq_text_printf(NULL, 1, x_base + 4, y_base + 9, "%s", get_osc_type_str(osc_type));
}

static void gui_draw_amp_env(display_context_t disp,
                             uint8_t wav_idx,
                             int x_base, int y_base)
{
    struct envelope_s env = waveforms[wav_idx].amp_env;
    int const box_width = 62;
    int const env_box_height = 140;

    if ((1 == selected_field_main) && (wav_idx == selected_wav_idx))
        rdpq_set_mode_fill(color_blue);
    else
        rdpq_set_mode_fill(color_gray);
    rdpq_fill_rectangle(x_base, y_base, x_base + box_width, y_base + env_box_height);

    x_base += 4;

    rdpq_text_printf(NULL, 1, x_base, y_base + env_box_height - 2, " A D S R ");
    x_base += 6;

    rdpq_set_mode_fill(color_white);

    int a_height = env.attack;
    int a_pos = y_base + (env_box_height - 10) - a_height;
    rdpq_fill_rectangle(x_base, a_pos, x_base + 6, a_pos + a_height);
    x_base += (2 * 6);

    int d_height = env.decay;
    int d_pos = y_base + (env_box_height - 10) - d_height;
    rdpq_fill_rectangle(x_base, d_pos, x_base + 6, d_pos + d_height);
    x_base += (2 * 6);

    int s_height = ((MIDI_MAX_DATA_BYTE * (uint64_t)env.sustain_level) / (uint64_t)UINT32_MAX);
    int s_pos = y_base + (env_box_height - 10) - s_height;
    rdpq_fill_rectangle(x_base, s_pos, x_base + 6, s_pos + s_height);
    x_base += (2 * 6);

    int r_height = env.release;
    int r_pos = y_base + (env_box_height - 10) - r_height;
    rdpq_fill_rectangle(x_base, r_pos, x_base + 6, r_pos + r_height);
}

static void gui_draw_amt_box(display_context_t disp,
                             uint8_t wav_idx,
                             int x_base, int y_base)
{
    int const box_width = 62;

    if ((2 == selected_field_main) && (wav_idx == selected_wav_idx))
        rdpq_set_mode_fill(color_blue);
    else
        rdpq_set_mode_fill(color_gray);
    rdpq_fill_rectangle(x_base, y_base, x_base + box_width, y_base + 10);
}

static char * get_osc_type_str(enum oscillator_type_e osc_type)
{
    switch (osc_type)
    {
        case SINE:
            return "SINE";
        case TRIANGLE:
            return "TRIANGLE";
        case SQUARE:
            return "SQUARE";
        case RAMP:
            return "RAMP";
        case NONE:
            return "NONE";
        default:
            return "Unknown";
    }
}

void gui_select_right(void)
{
    if ((NUM_WAVETABLES - 1) == selected_wav_idx)
        selected_wav_idx = 0;
    else
        ++selected_wav_idx;
}

void gui_select_left(void)
{
    if (0 == selected_wav_idx)
        selected_wav_idx = NUM_WAVETABLES - 1;
    else
        --selected_wav_idx;
}

void gui_select_up(void)
{
    if (0 == selected_field_main)
        selected_field_main = 2;
    else
        --selected_field_main;
}

void gui_select_down(void)
{
    if (2 == selected_field_main)
        selected_field_main = 0;
    else
        ++selected_field_main;
}