#include "input.h"

#include "voice.h"


#include <libdragon.h>
#include <midi.h>

#include <stddef.h>

static size_t midi_in_bytes = 0;
static uint32_t midi_rx_ctr = 0;
static uint8_t midi_in_buffer[MIDI_RX_PAYLOAD] = {0};

bool input_poll_and_handle(void)
{
    midi_in_bytes = midi_rx_poll(JOYPAD_PORT_1,
                                 midi_in_buffer,
                                 sizeof(midi_in_buffer));

    if (midi_in_bytes > 0)
    {
        ++midi_rx_ctr;
        handle_midi_input(midi_in_bytes);
        return true;
    }
    else
    {
        return false;
    }
}

void handle_midi_input(size_t midi_in_bytes)
{
    static midi_parser_state midi_parser = {0};

    midi_msg msg_buf[MIDI_RX_PAYLOAD] = {0};
    size_t num_msgs = midi_process_messages(&midi_parser, 
                                             midi_in_buffer, midi_in_bytes,
                                             msg_buf, sizeof(msg_buf));
    for (size_t msg_idx = 0; msg_idx < num_msgs; ++msg_idx)
    {
        midi_msg msg = msg_buf[msg_idx];

        if ((MIDI_NOTE_OFF == msg.status)
            || ((MIDI_NOTE_ON == msg.status) && (0 == msg.data[1])))
        {
            if (msg.data[0] >= MIDI_NOTE_MAX)
                continue;
            voice_t * voice = find_voice_to_close(msg.data[0]);
            if (voice)
            {
                note_off(voice);
            }
        }
        else if (MIDI_NOTE_ON == msg.status)
        {
            if (msg.data[0] >= MIDI_NOTE_MAX)
                continue;
            // TODO: Handle velocity
            voice_t * voice = find_next_voice();
            note_on(voice, msg.data[0]);
        }
    }
}
