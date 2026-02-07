#include <libdragon.h>

#include "audio_engine.h"
#include "init.h"
#include "input.h"
#include "gui.h"
#include "wavetable.h"
#include "voice.h"

int main(void)
{
    gui_init();
    input_init();
    wavetable_init();
    voice_init();
    audio_engine_init();

    // debug_init_isviewer();
    // debug_init_usblog();

    gui_draw_screen();

    uint8_t update_graphics_ctr = NUM_DISP_BUFFERS;

	while(1) {
        if (input_poll_and_handle())
        {
            update_graphics_ctr = NUM_DISP_BUFFERS;
        }

        if (update_graphics_ctr > 0)
        {
            gui_draw_screen();
            --update_graphics_ctr;
        }
        else if (peak > 0)
        {
            gui_draw_level_meter(NULL);
        }
    }

	return 0;
}
