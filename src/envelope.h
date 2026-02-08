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
    uint16_t attack;
    uint16_t decay;
    uint16_t sustain_level;
    uint16_t release;
};

#endif
