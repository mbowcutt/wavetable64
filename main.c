#include <libdragon.h>

#include "audio_engine.h"
#include "init.h"
#include "input.h"
#include "display.h"
#include "wavetable.h"
#include "voice.h"

int main(void)
{
	display_init(RESOLUTION_512x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);

    draw_splash(INIT);

    joypad_init();
    debug_init_isviewer();
    debug_init_usblog();

    if (!generate_wave_tables())
    {
        return -1;
    }

    draw_splash(GEN_FREQ_TBL);

    draw_splash(INIT_AUDIO);
    init_voices();
    audio_engine_init();

    graphics_draw();

	while(1) {
        if (input_poll_and_handle())
        {
            graphics_draw();
        }

        audio_buffer_run();
    }

    // if (mix_buffer) free(mix_buffer);

	return 0;
}
