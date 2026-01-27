#ifndef ENVELOPE_H
#define ENVELOPE_H

enum envelope_state_e {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
    NUM_ENVELOPE_STATES
};

struct envelope_s
{
    uint8_t attack;
    uint8_t decay;
    uint32_t sustain_level;
    uint8_t release;
};

#endif
