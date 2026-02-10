#ifndef ENVELOPE_H
#define ENVELOPE_H

/// Enum representing each envelope stage.
enum envelope_stage_e {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
    NUM_ENVELOPE_STAGES
};

/// Struct containing the envelope settings.
/// Sustain is represented as an absolute level between [0,UINT32_MAX].
/// Attack, decay, and release are represented as 14 bit numbers and can be set
/// between [0,MIDI_MAX_NRPN_VAL].
struct envelope_s
{
    uint16_t attack;
    uint16_t decay;
    uint32_t sustain_level;
    uint16_t release;
};

#endif
