#include "gui.h"

#include "init.h"
#include "audio_engine.h"
#include "envelope.h"
#include "voice.h"
#include "wavetable.h"

#include <libdragon.h>
#include <midi.h>
#include <rdpq.h>
#include <rdpq_rect.h>
#include <rdpq_text.h>

#include <stddef.h>

enum menu_screen_e
{
    SCREEN_MAIN,
    SCREEN_FILE,
    SCREEN_DEBUG,
    SCREEN_SETTINGS
};

enum main_sel_e
{
    MAIN_SEL_OSC_1,
    MAIN_SEL_OSC_2,

    MAIN_SEL_ENV_1,
    MAIN_SEL_ENV_2,
    MAIN_SEL_ENV_3,

    MAIN_SEL_LFO_1,
    MAIN_SEL_LFO_2,
};

enum main_osc_subsel_e
{
    MAIN_OSC_SUBSEL_SHAPE,
    MAIN_OSC_SUBSEL_AMP_ENV,
    MAIN_OSC_SUBSEL_GAIN,
};

enum main_env_subsel_e
{
    MAIN_ENV_SUBSEL_A,
    MAIN_ENV_SUBSEL_D,
    MAIN_ENV_SUBSEL_S,
    MAIN_ENV_SUBSEL_R,
};

enum main_lfo_subsel_e
{
    MAIN_LFO_SUBSEL_SHAPE,
    MAIN_LFO_SUBSEL_RATE,
    MAIN_LFO_SUBSEL_DEPTH,
    MAIN_LFO_SUBSEL_DST,
};

static struct {
    enum menu_screen_e screen;
    enum main_sel_e sel;
    bool selected;
    union
    {
        enum main_osc_subsel_e osc;
        enum main_env_subsel_e env;
        enum main_lfo_subsel_e lfo;
    } subsel;
} gui_state =
{
    .screen = SCREEN_MAIN,
    .sel = MAIN_SEL_OSC_1,
    .selected = false,
    .subsel.osc = MAIN_OSC_SUBSEL_SHAPE,
};

static void gui_print_header(display_context_t disp);
static void gui_print_footer(display_context_t disp);
static void gui_print_menu(display_context_t disp);
static void gui_print_main(display_context_t disp);

static void gui_draw_osc(uint8_t osc_idx, int x_base, int y_base);

static char * get_osc_shape_str(enum oscillator_shape_e osc_shape);

static void gui_main_nav_right(void);
static void gui_main_nav_left(void);
static void gui_main_nav_up(void);
static void gui_main_nav_down(void);

static void gui_osc_nav_left(void);
static void gui_osc_nav_right(void);
static void gui_osc_nav_up(void);
static void gui_osc_nav_down(void);

static color_t color_red = RGBA32(0xFF, 0, 0, 0xFF);
static color_t color_green = RGBA32(0, 0xFF, 0, 0xFF);
static color_t color_blue = RGBA32(0, 0, 0xFF, 0xFF);
static color_t color_yellow = RGBA32(0xFF, 0xFF, 0, 0xFF);
static color_t color_white = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);
static color_t color_black = RGBA32(0, 0, 0, 0xFF);
static color_t color_gray = RGBA32(0x44, 0x44, 0x44, 0xFF);

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

    switch (gui_state.screen)
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
    switch (gui_state.screen)
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

    size_t num_boxes = (peak * total_boxes) / INT16_MAX;

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

    for (uint8_t osc_idx = 0; osc_idx < NUM_OSCILLATORS; ++osc_idx)
    {
        gui_draw_osc(osc_idx, x_base, y_base);
        x_base += 100 + 8;
    }
}

static void gui_draw_osc(uint8_t osc_idx, int x_base, int y_base)
{
    if (((0 == osc_idx) && (MAIN_SEL_OSC_1 == gui_state.sel))
        || ((1 == osc_idx) && (MAIN_SEL_OSC_2 == gui_state.sel)))
    {
        rdpq_set_mode_fill(color_blue);
    }
    else
    {
        rdpq_set_mode_fill(color_gray);
    }
    rdpq_fill_rectangle(x_base - 4, y_base, x_base + 100, y_base + 62);

    if (gui_state.selected &&
        (((0 == osc_idx) && (MAIN_SEL_OSC_1 == gui_state.sel))
        || ((1 == osc_idx) && (MAIN_SEL_OSC_2 == gui_state.sel))))
    {
        rdpq_set_fill_color(color_green);
        switch (gui_state.subsel.osc)
        {
            case MAIN_OSC_SUBSEL_SHAPE:
                rdpq_fill_rectangle(x_base - 2, y_base + 1, x_base + 98, y_base + 12);
                break;
            case MAIN_OSC_SUBSEL_AMP_ENV:
                rdpq_fill_rectangle(x_base - 2, y_base + 40, x_base + 98, y_base + 51);
                break;
            case MAIN_OSC_SUBSEL_GAIN:
                rdpq_fill_rectangle(x_base - 2, y_base + 50, x_base + 98, y_base + 61);
                break;
        }
    }

    // TODO: Draw osc waveform
    rdpq_set_fill_color(color_black);
    rdpq_fill_rectangle(x_base + 4, y_base + 12, x_base + 92, y_base + 40);

    wavetable_t * osc = &oscillators[osc_idx];
    int gain_width = (62 * osc->gain) / MIDI_MAX_DATA_BYTE;
    rdpq_set_fill_color(color_white);
    rdpq_fill_rectangle(x_base + 32, y_base + 52, x_base + 32 + gain_width, y_base + 59);

    rdpq_text_printf(NULL, 1, x_base, y_base + 10,  "OSC %d: %s",
                     osc_idx + 1, get_osc_shape_str(osc->shape));
    rdpq_text_printf(NULL, 1, x_base, y_base + 49, "AMP ENV: ENV %d",
                     osc->amp_env_idx + 1);
    rdpq_text_print(NULL, 1, x_base, y_base + 59, "GAIN:");
}

void gui_screen_next(void)
{
    if (SCREEN_SETTINGS == gui_state.screen)
    {
        gui_state.screen = SCREEN_MAIN;
    }
    else
    {
        ++gui_state.screen;
    }
    gui_state.selected = false;
}

void gui_screen_prev(void)
{
    if (SCREEN_MAIN == gui_state.screen)
    {
        gui_state.screen = SCREEN_SETTINGS;
    }
    else
    {
        --gui_state.screen;
    }
    gui_state.selected = false;
}

static char * get_osc_shape_str(enum oscillator_shape_e osc_shape)
{
    switch (osc_shape)
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

void gui_nav_right(void)
{
    switch (gui_state.screen)
    {
        case SCREEN_MAIN:
            gui_main_nav_right();
            break;

        default:
            break;
    }
}

void gui_nav_left(void)
{
    switch (gui_state.screen)
    {
        case SCREEN_MAIN:
            gui_main_nav_left();
            break;

        default:
            break;
    }
}

#define ENV_RATE_GRANULE (MIDI_MAX_NRPN_VAL / MIDI_MAX_DATA_BYTE)
#define SUSTAIN_GRANULE (UINT32_MAX/ MIDI_MAX_DATA_BYTE)

void gui_nav_up(void)
{
    switch (gui_state.screen)
    {
        case SCREEN_MAIN:
            gui_main_nav_up();
            break;

        default:
            break;
    }
}

void gui_nav_down(void)
{
    switch (gui_state.screen)
    {
        case SCREEN_MAIN:
            gui_main_nav_down();
            break;

        default:
            break;
    }
}

static void gui_main_nav_right(void)
{
    if (gui_state.selected)
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
            case MAIN_SEL_OSC_2:
                gui_osc_nav_right();
                break;
            default:
                break;
        }
    }
    else
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
                gui_state.sel = MAIN_SEL_OSC_2;
                break;
            case MAIN_SEL_OSC_2:
                /// TODO: Navigate to envelopes
                break;
            default:
                break;
        }
    }
}

static void gui_main_nav_left(void)
{
    if (gui_state.selected)
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
            case MAIN_SEL_OSC_2:
                gui_osc_nav_left();
                break;
            default:
                break;
        }
    }
    else
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
                /// TODO: Navigate to envelopes
                break;
            case MAIN_SEL_OSC_2:
                gui_state.sel = MAIN_SEL_OSC_1;
                break;
            default:
                break;
        }
    }
}

static void gui_main_nav_up(void)
{
    if (gui_state.selected)
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
            case MAIN_SEL_OSC_2:
                gui_osc_nav_up();
                break;
            default:
                break;
        }
    }
    else
    {
        /// TODO: Navigate
    }
}

static void gui_main_nav_down(void)
{
    if (gui_state.selected)
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
            case MAIN_SEL_OSC_2:
                gui_osc_nav_down();
                break;
            default:
                break;
        }
    }
    else
    {
        /// TODO: Navigate
    }
}

static void gui_osc_nav_left(void)
{
    wavetable_t * osc = &oscillators[gui_state.sel - MAIN_SEL_OSC_1];
 
    switch (gui_state.subsel.osc)
    {
        case MAIN_OSC_SUBSEL_SHAPE:
            if (SINE == osc->shape)
            {
                osc->shape = NONE;
            }
            else
            {
                --osc->shape;
            }
            break;
        case MAIN_OSC_SUBSEL_AMP_ENV:
            if (0 == osc->amp_env_idx)
            {
                osc->amp_env_idx = NUM_ENVELOPES - 1;
            }
            else
            {
                --osc->amp_env_idx;
            }
            break;
        case MAIN_OSC_SUBSEL_GAIN:
            if (0 < osc->gain)
            {
                --osc->gain;
            }
            break;
        default:
            break;
    }
}

static void gui_osc_nav_right(void)
{
    wavetable_t * osc = &oscillators[gui_state.sel - MAIN_SEL_OSC_1];
    switch (gui_state.subsel.osc)
    {
        case MAIN_OSC_SUBSEL_SHAPE:
            if (NONE == osc->shape)
            {
                osc->shape = SINE;
            }
            else
            {
                ++osc->shape;
            }
            break;
        case MAIN_OSC_SUBSEL_AMP_ENV:
            if ((NUM_ENVELOPES - 1) == osc->amp_env_idx)
            {
                osc->amp_env_idx = 0;
            }
            else
            {
                ++osc->amp_env_idx;
            }
            break;
        case MAIN_OSC_SUBSEL_GAIN:
            if (MIDI_MAX_DATA_BYTE > osc->gain)
            {
                ++osc->gain;
            }
            break;
        default:
            break;
    }
}

static void gui_osc_nav_up(void)
{
    switch (gui_state.subsel.osc)
    {
        case MAIN_OSC_SUBSEL_SHAPE:
            break;
        case MAIN_OSC_SUBSEL_AMP_ENV:
            gui_state.subsel.osc = MAIN_OSC_SUBSEL_SHAPE;
            break;
        case MAIN_OSC_SUBSEL_GAIN:
            gui_state.subsel.osc = MAIN_OSC_SUBSEL_AMP_ENV;
            break;
    }
}

static void gui_osc_nav_down(void)
{
    switch (gui_state.subsel.osc)
    {
        case MAIN_OSC_SUBSEL_SHAPE:
            gui_state.subsel.osc = MAIN_OSC_SUBSEL_AMP_ENV;
            break;
        case MAIN_OSC_SUBSEL_AMP_ENV:
            gui_state.subsel.osc = MAIN_OSC_SUBSEL_GAIN;
            break;
        case MAIN_OSC_SUBSEL_GAIN:
            break;
    }
}

void gui_select(void)
{
    if (!gui_state.selected)
    {
        gui_state.selected = true;
    }
    else
    {
        gui_deselect();
    }
}

void gui_deselect(void)
{
    gui_state.selected = false;
}

bool gui_recv_continuous_input(void)
{
    bool ret = false;

    if (gui_state.selected
        && (((MAIN_SEL_OSC_1 == gui_state.sel)
             || (MAIN_SEL_OSC_2 == gui_state.sel))
            && (MAIN_OSC_SUBSEL_GAIN == gui_state.subsel.osc)))
    {
        ret = true;
    }
    return ret;
}
