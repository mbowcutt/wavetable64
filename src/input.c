#include "input.h"

#include "audio_engine.h"
#include "gui.h"
#include "voice.h"
#include "wavetable.h"

#include <libdragon.h>
#include <midi.h>

#include <stddef.h>

#define MIDI_CC_ATTACK 14
#define MIDI_CC_DECAY 15
#define MIDI_CC_SUSTAIN 13
#define MIDI_CC_RELEASE 12
#define MIDI_CC_GAIN 117

static size_t midi_in_bytes = 0;
static uint32_t midi_rx_ctr = 0;
static uint8_t midi_in_buffer[MIDI_RX_PAYLOAD] = {0};

static bool input_handle_midi(size_t midi_in_bytes);
static uint16_t nrpn = 0;

void input_init(void)
{
    joypad_init();
}

bool input_poll_and_handle(void)
{
    bool update_graphics = false;

    joypad_poll();
    joypad_buttons_t buttons = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    if (buttons.raw)
    {
        if (buttons.l)
        {
            gui_screen_prev();
            update_graphics = true;
        }
        else if (buttons.r)
        {
            gui_screen_next();
            update_graphics = true;
        }
        else if (buttons.d_right)
        {
            gui_select_right();
            update_graphics = true;
        }
        else if (buttons.d_left)
        {
            gui_select_left();
            update_graphics = true;
        }
        else if (buttons.d_up)
        {
            gui_select_up();
            update_graphics = true;
        }
        else if (buttons.d_down)
        {
            gui_select_down();
            update_graphics = true;
        }
        else if (buttons.a)
        {
            gui_select();
            update_graphics = true;
        }
        else if (buttons.b)
        {
            gui_deselect();
            update_graphics = true;
        }
    }
    else if (field_selected)
    {
        buttons = joypad_get_buttons_held(JOYPAD_PORT_1);
        if (buttons.raw)
        {
            if ((2 == selected_field_main) && buttons.d_right)
            {
                gui_select_right();
                update_graphics = true;
            }
            else if ((2 == selected_field_main) && buttons.d_left)
            {
                gui_select_left();
                update_graphics = true;
            }
            else if ((1 == selected_field_main) && buttons.d_up)
            {
                gui_select_up();
                update_graphics = true;
            }
            else if ((1 == selected_field_main) && buttons.d_down)
            {
                gui_select_down();
                update_graphics = true;
            }
        }
    }

    midi_in_bytes = midi_rx_poll(JOYPAD_PORT_1,
                                 midi_in_buffer,
                                 sizeof(midi_in_buffer));

    if (midi_in_bytes > 0)
    {
        ++midi_rx_ctr;
        if (input_handle_midi(midi_in_bytes))
        {
            update_graphics = true;
        }
    }

    return update_graphics;
}

static bool input_handle_midi(size_t midi_in_bytes)
{
    static midi_parser_state midi_parser = {0};

    bool update_graphics = false;

    midi_msg msg_buf[MIDI_RX_PAYLOAD] = {0};
    size_t num_msgs = midi_process_messages(&midi_parser, 
                                            midi_in_buffer, midi_in_bytes,
                                            msg_buf, sizeof(msg_buf));
    for (size_t msg_idx = 0; msg_idx < num_msgs; ++msg_idx)
    {
        midi_msg msg = msg_buf[msg_idx];

        if ((MIDI_NOTE_OFF == (msg.status & 0xF0))
            || ((MIDI_NOTE_ON == (msg.status & 0xF0)) && (0 == msg.data[1])))
        {
            if (msg.data[0] >= MIDI_MAX_DATA_BYTE)
                continue;
            voice_t * voice = voice_find_for_note_off(msg.data[0]);
            if (voice)
            {
                voice_note_off(voice);
            }
        }
        else if (MIDI_NOTE_ON == (msg.status & 0xF0))
        {
            if (msg.data[0] >= MIDI_MAX_DATA_BYTE)
                continue;
            // TODO: Handle velocity
            voice_t * voice = voice_find_next();
            voice_note_on(voice, msg.data[0]);
        }
        else if (MIDI_CONTROL_CHANGE == (msg.status & 0xF0))
        {
            switch (msg.data[0])
            {
                case MIDI_CC_ATTACK:
                    voice_envelope_set_attack(msg.data[1]);
                    update_graphics = true;
                    break;
                case MIDI_CC_DECAY:
                    voice_envelope_set_decay(msg.data[1]);
                    update_graphics = true;
                    break;
                case MIDI_CC_SUSTAIN:
                    voice_envelope_set_sustain(msg.data[1]);
                    update_graphics = true;
                    break;
                case MIDI_CC_RELEASE:
                    voice_envelope_set_release(msg.data[1]);
                    update_graphics = true;
                    break;
                case MIDI_CC_GAIN:
                    audio_engine_set_gain(msg.data[1]);
                    update_graphics = true;
                    break;
                case 99:
                    nrpn = ((uint16_t)msg.data[1]) << 7;
                    break;
                case 98:
                    nrpn |= msg.data[1];
                    break;
                case 6:
                    switch (nrpn)
                    {
                        case 0x0003:
                            switch (msg.data[1])
                            {
                                case 0:
                                    waveforms[0].osc = TRIANGLE;
                                    update_graphics = true;
                                    break;
                                case 1:
                                    waveforms[0].osc = SINE;
                                    update_graphics = true;
                                    break;
                                case 2:
                                    waveforms[0].osc = RAMP;
                                    update_graphics = true;
                                    break;
                                case 3:
                                    waveforms[0].osc = SQUARE;
                                    update_graphics = true;
                                    break;
                                default:
                                    break;
                            }
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }
    }

    return update_graphics;
}
