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

    gui_draw();

    bool update_graphics = false;

	while(1) {
        if (input_poll_and_handle())
        {
            update_graphics = true;
        }

        if (update_graphics || (0u != high_watermark))
        {
            gui_draw();
            update_graphics = false;
        }
    }

	return 0;
}
