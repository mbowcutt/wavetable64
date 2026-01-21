#include "gui.h"

#include "init.h"

#include <libdragon.h>

#include <stddef.h>

// static char * get_osc_type_str(void);

void gui_init(void)
{
    display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    gui_splash(INIT);
}

void gui_draw(void)
{
    // static char str_osc[64] = {0};
    // snprintf(str_osc, 64, "Oscillator: %s", get_osc_type_str());

    // static char str_gain[64] = {0};
    // snprintf(str_gain, 64, "Gain: %2.1f", mix_gain);

    // static char str_midi_data[64];
    // snprintf(str_midi_data, 64, "MIDI RX got %d bytes (%ld): ", midi_in_bytes, midi_rx_ctr);
    // for (uint8_t idx = 0; (idx < midi_in_bytes) && (idx < MIDI_RX_PAYLOAD); ++idx)
    // {
    //     char tmp[6];
    //     snprintf(tmp, 6, "0x%02X ", midi_in_buffer[idx]);
    //     strcat(str_midi_data, tmp);
    // }

#if DEBUG_AUDIO_BUFFER_STATS
    static char str_dbg_audio_buf_stats[64] = {0};
    snprintf(str_dbg_audio_buf_stats, 64, "Write cnt: %u\nLocal write cnt:%u\nNo write cnt: %u",
             write_cnt, local_write_cnt, no_write_cnt);
#endif

    display_context_t disp = display_get();
	graphics_fill_screen(disp, 0);
    graphics_draw_text(disp, 30, 10, "N64 Wavetable Synthesizer\t\t\t\t\tv0.1");
	graphics_draw_text(disp, 30, 18, "(c) 2026 Michael Bowcutt");
	// graphics_draw_text(disp, 30, 50, str_osc);
    // graphics_draw_text(disp, 30, 66, str_gain);
    // graphics_draw_text(disp, 30, 74, str_midi_data);
#if DEBUG_AUDIO_BUFFER_STATS
    graphics_draw_text(disp, 30, 74, str_dbg_audio_buf_stats);
#endif

	display_show(disp);
}

void gui_splash(enum init_state_e init_state)
{
    display_context_t disp = display_get();
    graphics_fill_screen(disp, 0);
    graphics_draw_text(disp, 30, 10, "N64 Wavetable Synthesizer\t\t\t\t\tv0.1");
    graphics_draw_text(disp, 30, 18, "(c) 2026 Michael Bowcutt");

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