#pragma once
#include <cstdint>

// Note/pitch conventions mirror libopenmpt's internal modcommand.h (stable enum values).
// Notes are 1..128, NOTE_MIDDLEC = 61 (= MIDI middle C). Emitted MIDI pitch = note + PITCH_OFFSET,
// so OpenMPT middle C (61) maps to MIDI 60 (standard MIDI).
const int OMPT_NOTE_MIN = 1;
const int OMPT_NOTE_MAX = 128;
const int OMPT_NOTE_MIDDLEC = 61;
const int OMPT_NOTE_FADE = 0xFD;    // ~~~
const int OMPT_NOTE_NOTECUT = 0xFE; // ^^^
const int OMPT_NOTE_KEYOFF = 0xFF;  // ===
const int PITCH_OFFSET = -OMPT_NOTE_MIN; // -1: OpenMPT note 1 (C-0) -> MIDI 0

// libopenmpt effect-column commands (CMD_*) we handle. Values mirror OpenMPT's EffectCommand enum.
enum OmptEffect : std::uint8_t
{
	CMD_NONE_ = 0,
	CMD_ARPEGGIO_ = 1,
	CMD_TONEPORTA_ = 4,    // 3xx: OpenMPT CMD_TONEPORTAMENTO (glide ignored; note column starts a new note at target pitch)
	CMD_TONEPORTAVOL_ = 6, // 5xy: tone porta + volume slide (volume same as Axy)
	CMD_VIBRATOVOL_ = 7,   // 6xy: vibrato + volume slide (volume same as Axy)
	CMD_OFFSET_ = 10,
	CMD_VOLUMESLIDE_ = 11, // Axy
	CMD_POSITIONJUMP_ = 12,
	CMD_VOLUME_ = 13,
	CMD_PATTERNBREAK_ = 14,
	CMD_RETRIG_ = 15,
	CMD_SPEED_ = 16,
	CMD_TEMPO_ = 17,
	CMD_MODCMDEX_ = 19,
	CMD_S3MCMDEX_ = 20,
	CMD_KEYOFF_ = 25,
};

// libopenmpt volume-column commands (VOLCMD_*) we handle. Values mirror OpenMPT's VolumeCommand enum.
enum OmptVolCmd : std::uint8_t
{
	VOLCMD_NONE_ = 0,
	VOLCMD_VOLUME_ = 1,
	VOLCMD_VOLSLIDEUP_ = 3,
	VOLCMD_VOLSLIDEDOWN_ = 4,
	VOLCMD_FINEVOLUP_ = 5,
	VOLCMD_FINEVOLDOWN_ = 6,
};
