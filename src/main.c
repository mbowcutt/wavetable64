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
        if (audio_engine_run())
        {
            update_graphics = true;
        }

        if (input_poll_and_handle())
        {
            update_graphics = true;
        }

        if (update_graphics)
        {
            gui_draw();
            update_graphics = false;
        }
    }

    // if (mix_buffer) free(mix_buffer);

	return 0;
}
