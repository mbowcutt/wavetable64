#include "input.h"

#include "audio_engine.h"
#include "gui.h"
#include "voice.h"
#include "wavetable.h"

#include <libdragon.h>
#include <midi64.h>

#include <stddef.h>

#define MIDI_CC_DATA_ENTRY_MSB 6
#define MIDI_CC_NRPN_LSB 98
#define MIDI_CC_NRPN_MSB 99

#define MIDI_CC_ENV1_ATTACK 14
#define MIDI_CC_ENV1_DECAY 15
#define MIDI_CC_ENV1_SUSTAIN 13
#define MIDI_CC_ENV1_RELEASE 12
#define MIDI_CC_GAIN 117
#define MIDI_NRPN_OSC1_SHAPE 0x0003

#define MIDI_NOTE_D3 50

static size_t midi_in_bytes = 0;
static uint32_t midi_rx_ctr = 0;
static uint8_t midi_in_buffer[MIDI_RX_PAYLOAD] = {0};

static uint16_t nrpn = 0;
static uint8_t midi_ch_basic = 0;
static bool ocarina_mode = false;
static uint8_t ocarina_root = MIDI_NOTE_D3;

static bool input_handle_midi(size_t midi_in_bytes);
static bool handle_button_press(joypad_buttons_t buttons);
static bool handle_buttons_held(joypad_buttons_t buttons);
static bool handle_ocarina(void);
static void handle_ocarina_note_press(void);
static void handle_ocarina_shoulder_press(void);
static void handle_ocarina_shoulder_release(void);

void input_init(void)
{
    joypad_init();
}


size_t input_get_midi_in_byte_count(void)
{
    return midi_in_bytes;
}

uint32_t input_get_midi_rx_ctr(void)
{
    return midi_rx_ctr;
}

bool input_poll_and_handle(void)
{
    bool update_graphics = false;

    joypad_poll();
    joypad_buttons_t buttons_pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    joypad_buttons_t buttons_released = joypad_get_buttons_released(JOYPAD_PORT_1);
    joypad_buttons_t buttons_held = joypad_get_buttons_held(JOYPAD_PORT_1);

    if (!ocarina_mode)
    {
        if (buttons_pressed.raw)
        {
            update_graphics = handle_button_press(buttons_pressed);
        }
        else if (buttons_held.raw && gui_recv_continuous_input(buttons_held))
        {
            update_graphics = handle_buttons_held(buttons_held);
        }
    }
    else
    {
        if (buttons_pressed.start)
        {
            ocarina_mode = false;
            voice_all_off();
            voice_set_mono(false);
            update_graphics = true;
        }
        else
        {
            update_graphics = handle_ocarina();
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

static bool handle_button_press(joypad_buttons_t buttons)
{
    bool update_graphics = false;

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
        gui_nav_right();
        update_graphics = true;
    }
    else if (buttons.d_left)
    {
        gui_nav_left();
        update_graphics = true;
    }
    else if (buttons.d_up)
    {
        gui_nav_up();
        update_graphics = true;
    }
    else if (buttons.d_down)
    {
        gui_nav_down();
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
    else if (buttons.start)
    {
        ocarina_mode = true;
        voice_all_off();
        voice_set_mono(true);
    }

    return update_graphics;
}

static bool handle_buttons_held(joypad_buttons_t buttons)
{
    bool update_graphics = false;

    if (buttons.d_right)
    {
        gui_nav_right();
        update_graphics = true;
    }
    else if (buttons.d_left)
    {
        gui_nav_left();
        update_graphics = true;
    }
    else if (buttons.d_up)
    {
        gui_nav_up();
        update_graphics = true;
    }
    else if (buttons.d_down)
    {
        gui_nav_down();
        update_graphics = true;
    }

    return update_graphics;
}

static bool handle_ocarina(void)
{
    bool update_graphics = false;
    joypad_buttons_t btn_press = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    joypad_buttons_t btn_release = joypad_get_buttons_released(JOYPAD_PORT_1);
    joypad_buttons_t btn_held = joypad_get_buttons_held(JOYPAD_PORT_1);

    if (btn_press.a
        || btn_press.c_down
        || btn_press.c_right
        || btn_press.c_left
        || btn_press.c_up)
    {
        handle_ocarina_note_press();
        update_graphics = true;
    }

    if (btn_press.l
        || btn_press.z
        || btn_press.r)
    {
        handle_ocarina_shoulder_press();
        update_graphics = true;
    }

    if (btn_release.l
        || btn_release.z
        || btn_release.r)
    {
        handle_ocarina_shoulder_release();
        update_graphics = true;
    }

    if (btn_release.a
        || btn_release.c_down
        || btn_release.c_right
        || btn_release.c_left
        || btn_release.c_up)
    {
        if ( ! (btn_press.a
                || btn_press.c_down
                || btn_press.c_right
                || btn_press.c_left
                || btn_press.c_up
                || btn_held.a
                || btn_held.c_down
                || btn_held.c_right
                || btn_held.c_left
                || btn_held.c_up) )
        {
            voice_all_off();
            update_graphics = true;
        }
    }

    // TODO: Analog stick mod pitch & vibrato
    joypad_inputs_t inputs = joypad_get_inputs(JOYPAD_PORT_1);
    (void) inputs.stick_x;
    (void) inputs.stick_y;

    return update_graphics;
}

static void handle_ocarina_note_press(void)
{
    uint8_t note = 0xFF;

    joypad_buttons_t btn_press = joypad_get_buttons_pressed(JOYPAD_PORT_1);

    if (btn_press.a)
    {
        note = ocarina_root;
    }
    else if (btn_press.c_down)
    {
        note = ocarina_root + 3;
    }
    else if (btn_press.c_right)
    {
        note = ocarina_root + 7;
    }
    else if (btn_press.c_left)
    {
        note = ocarina_root + 9;
    }
    else if (btn_press.c_up)
    {
        note = ocarina_root + 12;
    }

    if (note != 0xFF)
    {
        joypad_buttons_t btn_held = joypad_get_buttons_held(JOYPAD_PORT_1);
        if (btn_held.r)
        {
            note += 1;
        }
        if (btn_held.l | btn_held.z)
        {
            note -= 1;
        }
    }

    if (note != 0xFF)
    {
        voice_note_on(note, MIDI_MAX_DATA_BYTE);
    }
}

static void handle_ocarina_shoulder_press(void)
{
    joypad_buttons_t btn_press = joypad_get_buttons_pressed(JOYPAD_PORT_1);
    joypad_buttons_t btn_held = joypad_get_buttons_held(JOYPAD_PORT_1);

    voice_t * voice = voice_get(0);
    uint8_t new_note = voice->note;

    if (btn_press.r)
    {
        new_note += 1;
    }
    if (btn_press.l || btn_press.z)
    {
        new_note -= 1;
    }

    if (btn_held.a
        || btn_held.c_down
        || btn_held.c_right
        || btn_held.c_left
        || btn_held.c_up)
    {
        voice_note_on(new_note, MIDI_MAX_DATA_BYTE);
    }
    else
    {
        voice->note = new_note;
        voice->tune = wavetable_get_midi_tune(new_note);
    }
}

static void handle_ocarina_shoulder_release(void)
{
    joypad_buttons_t btn_release = joypad_get_buttons_released(JOYPAD_PORT_1);
    joypad_buttons_t btn_held = joypad_get_buttons_held(JOYPAD_PORT_1);

    voice_t * voice = voice_get(0);
    uint8_t new_note = voice->note;

    if (btn_release.r)
    {
        new_note -= 1;
    }
    if (btn_release.l || btn_release.z)
    {
        new_note += 1;
    }

    if (btn_held.a
        || btn_held.c_down
        || btn_held.c_right
        || btn_held.c_left
        || btn_held.c_up)
    {
        voice_note_on(new_note, MIDI_MAX_DATA_BYTE);
    }
    else
    {
        voice->note = new_note;
        voice->tune = wavetable_get_midi_tune(new_note);
    }
}

uint8_t input_get_basic_channel(void)
{
    return midi_ch_basic;
}

void input_set_basic_channel(uint8_t channel)
{
    midi_ch_basic = channel;
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

        if ((msg.status == (MIDI_NOTE_OFF | midi_ch_basic))
            || ((msg.status == (MIDI_NOTE_ON | midi_ch_basic)) && (0 == msg.data[1])))
        {
            if (msg.data[0] < MIDI_MAX_DATA_BYTE)
            {
                voice_note_off(msg.data[0]);
            }
        }
        else if (msg.status == (MIDI_NOTE_ON | midi_ch_basic))
        {
            if ((msg.data[0] < MIDI_MAX_DATA_BYTE)
                || (msg.data[1] < MIDI_MAX_DATA_BYTE))
            {
                voice_note_on(msg.data[0], msg.data[1]);
            }
        }
        else if (msg.status == (MIDI_CONTROL_CHANGE | midi_ch_basic))
        {
            switch (msg.data[0])
            {
                case MIDI_CC_ENV1_ATTACK:
                    envelope_set_attack(0, ((uint16_t)msg.data[1] << 7));
                    update_graphics = true;
                    break;
                case MIDI_CC_ENV1_DECAY:
                    envelope_set_decay(0, ((uint16_t)msg.data[1] << 7));
                    update_graphics = true;
                    break;
                case MIDI_CC_ENV1_SUSTAIN:
                    envelope_set_sustain(0, ((((uint64_t)msg.data[1]) << 7) * UINT32_MAX) / MIDI_MAX_NRPN_VAL);
                    update_graphics = true;
                    break;
                case MIDI_CC_ENV1_RELEASE:
                    envelope_set_release(0, ((uint16_t)msg.data[1] << 7));
                    update_graphics = true;
                    break;
                case MIDI_CC_GAIN:
                    audio_engine_set_gain(msg.data[1]);
                    update_graphics = true;
                    break;
                case MIDI_CC_NRPN_MSB:
                    nrpn = ((uint16_t)msg.data[1]) << 7;
                    break;
                case MIDI_CC_NRPN_LSB:
                    nrpn |= msg.data[1];
                    break;
                case MIDI_CC_DATA_ENTRY_MSB:
                    switch (nrpn)
                    {
                        case MIDI_NRPN_OSC1_SHAPE:
                            switch (msg.data[1])
                            {
                                case 0:
                                    oscillators[0].shape = TRIANGLE;
                                    update_graphics = true;
                                    break;
                                case 1:
                                    oscillators[0].shape = SINE;
                                    update_graphics = true;
                                    break;
                                case 2:
                                    oscillators[0].shape = RAMP;
                                    update_graphics = true;
                                    break;
                                case 3:
                                    oscillators[0].shape = SQUARE;
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
                case MIDI_MODE_ALL_SOUND_OFF:
                    voice_all_kill();
                    break;
                case MIDI_MODE_ALL_NOTES_OFF:
                    // TODO: Only turn MIDI notes off
                    voice_all_off();
                    break;
                case MIDI_MODE_POLY_OFF:
                    voice_set_mono(false);
                    break;
                case MIDI_MODE_POLY_ON:
                    voice_set_mono(true);
                    break;
                default:
                    break;
            }
        }
    }

    return update_graphics;
}
