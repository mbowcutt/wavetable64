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
    SCREEN_OSC_ENV,
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
    .screen = SCREEN_OSC_ENV,
    .sel = MAIN_SEL_OSC_1,
    .selected = false,
    .subsel.osc = MAIN_OSC_SUBSEL_SHAPE,
};

static void gui_draw_header(display_context_t disp);
static void gui_draw_footer(display_context_t disp);
static void gui_draw_menu(display_context_t disp);
static void gui_draw_osc_env(display_context_t disp);

static void gui_draw_osc(uint8_t osc_idx, int x_base, int y_base);
static void gui_draw_env(uint8_t env_idx, int x_base, int y_base);

static char * get_osc_shape_str(enum oscillator_shape_e osc_shape);

static void gui_nav_osc_env_right(void);
static void gui_nav_osc_env_left(void);
static void gui_nav_osc_env_up(void);
static void gui_nav_osc_env_down(void);

static void gui_nav_osc_left(void);
static void gui_nav_osc_right(void);
static void gui_nav_osc_up(void);
static void gui_nav_osc_down(void);

static void gui_nav_env_left(void);
static void gui_nav_env_right(void);
static void gui_nav_env_up(void);
static void gui_nav_env_down(void);

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

	gui_draw_header(disp);
    gui_draw_footer(disp);
    gui_draw_menu(disp);

    switch (gui_state.screen)
    {
        case SCREEN_OSC_ENV:
            gui_draw_osc_env(disp);
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

    gui_draw_header(disp);
    gui_draw_footer(disp);

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

static void gui_draw_header(display_context_t disp)
{
    rdpq_set_mode_fill(color_blue);
    rdpq_fill_rectangle(0, 0, 512, 18);
    rdpq_text_print(NULL, 1, 214, 16, "wavtable64");
    // graphics_draw_line(disp, 0, 18, 512, 18, graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF));
}

static void gui_draw_footer(display_context_t disp)
{
    rdpq_set_mode_fill(color_gray);
    rdpq_fill_rectangle(0, 218, 512, 240);
    // graphics_draw_line(disp, 0, 218, 512, 218, graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF));
    rdpq_text_print(NULL, 1, 128, 230, "(c) 2026 Michael Bowcutt <mwbowcutt@gmail.com>");
}

static void gui_draw_menu(display_context_t disp)
{
    rdpq_set_mode_fill(color_gray);
    rdpq_fill_rectangle(0, 19, 512, 30);
    // graphics_draw_line(disp, 0, 30, 512, 30, graphics_make_color(0xFF, 0xFF, 0xFF, 0xFF));

    rdpq_set_fill_color(color_red);
    switch (gui_state.screen)
    {
        case SCREEN_OSC_ENV:
            rdpq_fill_rectangle(108, 19, 170, 30);
            break;
        case SCREEN_FILE:
            rdpq_fill_rectangle(196, 19, 228, 30);
            break;
        case SCREEN_DEBUG:
            rdpq_fill_rectangle(254, 19, 292, 30);
            break;
        case SCREEN_SETTINGS:
            rdpq_fill_rectangle(318, 19, 374, 30);
            break;
        default:
            break;
    }

    rdpq_text_print(NULL, 1, 28, 28, "<< L");
    rdpq_text_print(NULL, 1, 112, 28, "OSC & ENV");
    rdpq_text_print(NULL, 1, 200, 28, "FILE");
    rdpq_text_print(NULL, 1, 258, 28, "DEBUG");
    rdpq_text_print(NULL, 1, 322, 28, "SETTINGS");
    rdpq_text_print(NULL, 1, 488, 28, "R >>");

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

static void gui_draw_osc_env(display_context_t disp)
{
    int x_base = 26;
    int y_base = 34;

    for (uint8_t osc_idx = 0; osc_idx < NUM_OSCILLATORS; ++osc_idx)
    {
        gui_draw_osc(osc_idx, x_base, y_base);
        x_base += 100 + 8;
    }

    x_base = 242;
    y_base = 34;

    for (uint8_t env_idx = 0; env_idx < NUM_ENVELOPES; ++env_idx)
    {
        gui_draw_env(env_idx, x_base, y_base);

        y_base += 58;
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

    if (gui_state.selected
        && (((0 == osc_idx) && (MAIN_SEL_OSC_1 == gui_state.sel))
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

static void gui_draw_env(uint8_t env_idx, int x_base, int y_base)
{
    rdpq_set_mode_fill(color_gray);
    if (((0 == env_idx) && (MAIN_SEL_ENV_1 == gui_state.sel))
        || ((1 == env_idx) && (MAIN_SEL_ENV_2 == gui_state.sel))
        || ((2 == env_idx) && (MAIN_SEL_ENV_3 == gui_state.sel)))
    {
        rdpq_set_mode_fill(color_blue);
    }
    else
    {
        rdpq_set_mode_fill(color_gray);
    }
    rdpq_fill_rectangle(x_base - 4, y_base, x_base + 244, y_base + 56);

    if (gui_state.selected
        && (((env_idx == 0) && (MAIN_SEL_ENV_1 == gui_state.sel))
            || ((env_idx == 1) && (MAIN_SEL_ENV_2 == gui_state.sel))
            || ((env_idx == 2) && (MAIN_SEL_ENV_3 == gui_state.sel))))
    {
        rdpq_set_fill_color(color_green);
        switch (gui_state.subsel.env)
        {
            case MAIN_ENV_SUBSEL_A:
                rdpq_fill_rectangle(x_base - 2, y_base + 45, x_base + 56, y_base + 55);
                break;
            case MAIN_ENV_SUBSEL_D:
                rdpq_fill_rectangle(x_base + 58, y_base + 45, x_base + 116, y_base + 55);
                break;
            case MAIN_ENV_SUBSEL_S:
                rdpq_fill_rectangle(x_base + 118, y_base + 45, x_base + 176, y_base + 55);
                break;
            case MAIN_ENV_SUBSEL_R:
                rdpq_fill_rectangle(x_base + 178, y_base + 45, x_base + 236, y_base + 55);
                break;
            default:
                break;
        }
    }

    rdpq_set_fill_color(color_white);
    float a1[] = {(float)x_base, (float)(y_base + 44)};
    float a2[] = {x_base + (60 * ((float)envelopes[env_idx].attack / MIDI_MAX_NRPN_VAL)), (float)(y_base + 4)};
    float a3[] = {a2[0], a1[1]};
    float d1[] = {a2[0], a2[1]}; // same as a2
    float d2[] = {a2[0], y_base + 4 + (40 * ((float)(UINT32_MAX - envelopes[env_idx].sustain_level) / UINT32_MAX))};
    float d3[] = {a2[0] + (60 * ((float)envelopes[env_idx].decay / MIDI_MAX_NRPN_VAL)), d2[1]};
    float r1[] = {(float)x_base + 240, a1[1]};
    float r2[] = {r1[0] - (60 * ((float)envelopes[env_idx].release / MIDI_MAX_NRPN_VAL)), a1[1]};
    float r3[] = {r2[0], d2[1]};

    rdpq_triangle(&TRIFMT_FILL, a1, a2, a3);
    rdpq_triangle(&TRIFMT_FILL, d1, d2, d3);
    rdpq_triangle(&TRIFMT_FILL, r3, r2, r1);
    rdpq_fill_rectangle(a2[0], d2[1], r2[0], a1[1]);

    rdpq_text_printf(NULL, 1, x_base + 208, y_base + 14, "ENV %d",
                     env_idx + 1);
    rdpq_text_printf(NULL, 1, x_base,       y_base + 54, "A:%lums",
                     (uint32_t)(env_sample_lut[envelopes[env_idx].attack] / 44.1f));
    rdpq_text_printf(NULL, 1, x_base + 60,  y_base + 54, "D:%lums",
                     (uint32_t)(env_sample_lut[envelopes[env_idx].decay] / 44.1f));
    rdpq_text_printf(NULL, 1, x_base + 120, y_base + 54, "S: %2.1f%%",
                     (float)(envelopes[env_idx].sustain_level) * 100 / UINT32_MAX);
    rdpq_text_printf(NULL, 1, x_base + 180, y_base + 54, "R:%lums",
                     (uint32_t)(env_sample_lut[envelopes[env_idx].release] / 44.1f));
}

void gui_screen_next(void)
{
    if (SCREEN_SETTINGS == gui_state.screen)
    {
        gui_state.screen = SCREEN_OSC_ENV;
    }
    else
    {
        ++gui_state.screen;
    }
    gui_state.selected = false;
}

void gui_screen_prev(void)
{
    if (SCREEN_OSC_ENV == gui_state.screen)
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
        case SCREEN_OSC_ENV:
            gui_nav_osc_env_right();
            break;

        default:
            break;
    }
}

void gui_nav_left(void)
{
    switch (gui_state.screen)
    {
        case SCREEN_OSC_ENV:
            gui_nav_osc_env_left();
            break;

        default:
            break;
    }
}

void gui_nav_up(void)
{
    switch (gui_state.screen)
    {
        case SCREEN_OSC_ENV:
            gui_nav_osc_env_up();
            break;

        default:
            break;
    }
}

void gui_nav_down(void)
{
    switch (gui_state.screen)
    {
        case SCREEN_OSC_ENV:
            gui_nav_osc_env_down();
            break;

        default:
            break;
    }
}

static void gui_nav_osc_env_right(void)
{
    if (gui_state.selected)
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
            case MAIN_SEL_OSC_2:
                gui_nav_osc_right();
                break;
            case MAIN_SEL_ENV_1:
            case MAIN_SEL_ENV_2:
            case MAIN_SEL_ENV_3:
                gui_nav_env_right();
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
                gui_state.sel = MAIN_SEL_ENV_1;
                gui_state.subsel.env = MAIN_ENV_SUBSEL_A;
                break;
            default:
                break;
        }
    }
}

static void gui_nav_osc_env_left(void)
{
    if (gui_state.selected)
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
            case MAIN_SEL_OSC_2:
                gui_nav_osc_left();
                break;
            case MAIN_SEL_ENV_1:
            case MAIN_SEL_ENV_2:
            case MAIN_SEL_ENV_3:
                gui_nav_env_left();
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
                break;
            case MAIN_SEL_OSC_2:
                gui_state.sel = MAIN_SEL_OSC_1;
                break;
            case MAIN_SEL_ENV_1:
            case MAIN_SEL_ENV_2:
            case MAIN_SEL_ENV_3:
                gui_state.sel = MAIN_SEL_OSC_2;
                gui_state.subsel.osc = MAIN_OSC_SUBSEL_SHAPE;
            default:
                break;
        }
    }
}

static void gui_nav_osc_env_up(void)
{
    if (gui_state.selected)
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
            case MAIN_SEL_OSC_2:
                gui_nav_osc_up();
                break;
            case MAIN_SEL_ENV_1:
            case MAIN_SEL_ENV_2:
            case MAIN_SEL_ENV_3:
                gui_nav_env_up();
                break;
            default:
                break;
        }
    }
    else
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_ENV_2:
                gui_state.sel = MAIN_SEL_ENV_1;
                break;
            case MAIN_SEL_ENV_3:
                gui_state.sel = MAIN_SEL_ENV_2;
                break;
            default:
                break;
        }
    }
}

static void gui_nav_osc_env_down(void)
{
    if (gui_state.selected)
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_OSC_1:
            case MAIN_SEL_OSC_2:
                gui_nav_osc_down();
                break;
            case MAIN_SEL_ENV_1:
            case MAIN_SEL_ENV_2:
            case MAIN_SEL_ENV_3:
                gui_nav_env_down();
                break;
            default:
                break;
        }
    }
    else
    {
        switch (gui_state.sel)
        {
            case MAIN_SEL_ENV_1:
                gui_state.sel = MAIN_SEL_ENV_2;
                break;
            case MAIN_SEL_ENV_2:
                gui_state.sel = MAIN_SEL_ENV_3;
                break;
            default:
                break;
        }
    }
}

static void gui_nav_osc_left(void)
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

static void gui_nav_osc_right(void)
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

static void gui_nav_osc_up(void)
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

static void gui_nav_osc_down(void)
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

#define ENV_RATE_GRANULE (MIDI_MAX_NRPN_VAL / MIDI_MAX_DATA_BYTE)
#define SUSTAIN_GRANULE (UINT32_MAX/ MIDI_MAX_DATA_BYTE)

static void gui_nav_env_left(void)
{
    switch (gui_state.subsel.env)
    {
        case MAIN_ENV_SUBSEL_D:
            gui_state.subsel.env = MAIN_ENV_SUBSEL_A;
            break;

        case MAIN_ENV_SUBSEL_S:
            gui_state.subsel.env = MAIN_ENV_SUBSEL_D;
            break;

        case MAIN_ENV_SUBSEL_R:
            gui_state.subsel.env = MAIN_ENV_SUBSEL_S;
            break;

        default:
            break;
    }
}

static void gui_nav_env_right(void)
{
    switch (gui_state.subsel.env)
    {
        case MAIN_ENV_SUBSEL_A:
            gui_state.subsel.env = MAIN_ENV_SUBSEL_D;
            break;

        case MAIN_ENV_SUBSEL_D:
            gui_state.subsel.env = MAIN_ENV_SUBSEL_S;
            break;

        case MAIN_ENV_SUBSEL_S:
            gui_state.subsel.env = MAIN_ENV_SUBSEL_R;
            break;

        default:
            break;
    }
}

static void gui_nav_env_down(void)
{
    struct envelope_s * env = &envelopes[gui_state.sel - MAIN_SEL_ENV_1];
    switch (gui_state.subsel.env)
    {
        case MAIN_ENV_SUBSEL_A:
            if (ENV_RATE_GRANULE <= env->attack)
            {
                env->attack -= ENV_RATE_GRANULE;
            }
            else
            {
                env->attack = 0;
            }
            break;

        case MAIN_ENV_SUBSEL_D:
            if (ENV_RATE_GRANULE <= env->decay)
            {
                env->decay -= ENV_RATE_GRANULE;
            }
            else
            {
                env->decay = 0;
            }
            break;

        case MAIN_ENV_SUBSEL_S:
            if (SUSTAIN_GRANULE <= env->sustain_level)
            {
                env->sustain_level -= SUSTAIN_GRANULE;
            }
            else
            {
                env->sustain_level = 0;
            }
            break;

        case MAIN_ENV_SUBSEL_R:
            if (ENV_RATE_GRANULE <= env->release)
            {
                env->release -= ENV_RATE_GRANULE;
            }
            else
            {
                env->release = 0;
            }
            break;

        default:
            break;
    }
}

static void gui_nav_env_up(void)
{
    struct envelope_s * env = &envelopes[gui_state.sel - MAIN_SEL_ENV_1];
    switch (gui_state.subsel.env)
    {
        case MAIN_ENV_SUBSEL_A:
            if ((MIDI_MAX_NRPN_VAL - ENV_RATE_GRANULE) >= env->attack)
            {
                env->attack += ENV_RATE_GRANULE;
            }
            else
            {
                env->attack = MIDI_MAX_NRPN_VAL;
            }
            break;

        case MAIN_ENV_SUBSEL_D:
            if ((MIDI_MAX_NRPN_VAL - ENV_RATE_GRANULE) >= env->decay)
            {
                env->decay += ENV_RATE_GRANULE;
            }
            else
            {
                env->decay = MIDI_MAX_NRPN_VAL;
            }
            break;
        case MAIN_ENV_SUBSEL_S:
            if ((UINT32_MAX - SUSTAIN_GRANULE) >= env->sustain_level)
            {
                env->sustain_level += SUSTAIN_GRANULE;
            }
            else
            {
                env->sustain_level = UINT32_MAX;
            }
            break;
        case MAIN_ENV_SUBSEL_R:
            if ((MIDI_MAX_NRPN_VAL - ENV_RATE_GRANULE) >= env->release)
            {
                env->release += ENV_RATE_GRANULE;
            }
            else
            {
                env->release = MIDI_MAX_NRPN_VAL;
            }
            break;
        default:
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

bool gui_recv_continuous_input(joypad_buttons_t buttons_pressed)
{
    bool ret = false;

    if (gui_state.selected)
    {
        if ((buttons_pressed.d_left || buttons_pressed.d_right)
            && (((MAIN_SEL_OSC_1 == gui_state.sel) || (MAIN_SEL_OSC_2 == gui_state.sel))
                && (MAIN_OSC_SUBSEL_GAIN == gui_state.subsel.osc)))
        {
            ret = true;
        }
        else if ((buttons_pressed.d_up || buttons_pressed.d_down)
                 && ((MAIN_SEL_ENV_1 == gui_state.sel)
                     || (MAIN_SEL_ENV_2 == gui_state.sel)
                     || (MAIN_SEL_ENV_3 == gui_state.sel)))
        {
            ret = true;
        }
    }
    return ret;
}
