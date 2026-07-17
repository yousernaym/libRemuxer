#pragma once
#include "song.h"
#include "libRemuxer.h"
#include "SongReader.h"
#include <libopenmpt/libopenmpt.hpp>
#include <libopenmpt/libopenmpt_ext.hpp>
#include <cstdint>
#include <memory>
#include <vector>

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
	CMD_OFFSET_ = 10,
	CMD_VOLUMESLIDE_ = 11,
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

struct Loop
{
	int startR = 0;
	int loops = 0;
};


struct CellInfo
{
	int ins = 0;
	int note = 0;          // MIDI-space pitch (note + PITCH_OFFSET), 0 = no note
	bool keyOff = false;   // note column carried a key-off / note-cut / fade
	int command = 0;       // CMD_* (effect column)
	int param = 0;         // effect-column parameter
	int volcmd = 0;        // VOLCMD_* (volume column)
	int vol = 0;           // volume-column parameter
	int noteStartOffset = 0;
	int noteEndOffset = -1;
	int retriggerOffset = -1;
	int arpPitches[3] = { 0, 0, 0 };
	int volSlideVel = 0;
	int volSlideVelScale = 0;
	unsigned sampleOffset = 0;
};


struct RunningCellInfo
{
	int ins = 0;
	int note = 0;
	int noteStartT = 0;
	double noteStartS = 0;
	double volEnvStartS = 0;
	double sampleStartS = 0;
	bool loopSample = false;
	double sampleLength = 0;
	double sampleC5Speed = 8363;
	int volEnvEnd = 0;
	int startVol = 64;
	bool samplePlaying = false;
	bool volEnvEnded = false;
	int volSlideMem = 0;  // CMD_VOLUMESLIDE param memory
	int offsetMem = 0;    // CMD_OFFSET param memory
	Loop loop;
};

class ModReader : public SongReader
{
	// One audio render pass. channel < 0 => mixdown pass. channel >= 0 => render only that mod
	// channel: per-channel mode saves it whole as track `midiTrack`; per-instrument mode splices it.
	struct Pass
	{
		int midiTrack; // whole-track WAV target (per-channel mode); unused (-1) for splice passes
		int channel;   // mod channel to render, or -1 for the mixdown pass
	};
	// Tick->seconds is piecewise-linear over the tempo map (a mod "tick" = 1/24 beat = 2.5/tempo s,
	// independent of speed). Snapshotted before createNoteList scales the tempo times in place.
	struct TempoSeg
	{
		int startTick;      // pre-scale (24-tpb) tick this tempo starts at
		double startS;      // seconds elapsed at startTick
		double secPerTick;  // 2.5 / tempo
	};

	const double semitone = 1.0594630943593; //12th root of 2
	const int FadeOutTimeS = 7;
	std::vector<RunningCellInfo> runningRowInfo;
	std::vector<CellInfo> curRowInfo;
	std::unique_ptr<openmpt::module_ext> omptModule;
	openmpt::ext::interactive *interactive = nullptr;
	std::vector<Pass> passList;
	std::vector<TempoSeg> tempoSegs;
	int curPass = 0;
	float passFraction = 0;
	int curSongSpeed;
	int ptnDelay;
	int timeT = 0;
	double timeS = 0;
	double rowDur = 0;
	double tickDur = 0;
	int ptnJump = -1;
	int ptnStart = 0;
	SongData *songData;
	bool isFadingOut;
public:
	ModReader(Song &song);
	~ModReader();
	double getRowDur(double tempo, double speed);
	void readCell(int pattern, int row, int channel, CellInfo &cellInfo, RunningCellInfo &runningCellInfo);
	bool readCellFx(RunningTickInfo &firstTick, CellInfo &cellInfo, RunningCellInfo &runningCellInfo, int order, int row);
	void updateCell(RunningTickInfo &firstTick, const CellInfo &cellInfo, RunningCellInfo &runningCellInfo);
	void updateCellTicks(Song::Track &track, const CellInfo &cellInfo, RunningCellInfo &runningCellInfo);
	void extractNotes();
	void beginProcessing(const UserArgs &args);
	void buildTempoSegs();               // snapshot tick->seconds before createNoteList scales the tempo map
	double tickToSeconds(int tick) const;
	bool renderPassChunk();      // renders one audio chunk into wav; returns true when the current pass is complete
	void setupTrackPass(int channel);    // mute all mod channels except `channel`
	float process() override;
	void endProcessing() override;
};
