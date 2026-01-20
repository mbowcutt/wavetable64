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

	while(1) {
        if (input_poll_and_handle())
        {
            gui_draw();
        }

        audio_engine_run();
    }

    // if (mix_buffer) free(mix_buffer);

	return 0;
}
