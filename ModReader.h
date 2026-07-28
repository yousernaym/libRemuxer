#pragma once
#include "OmptCommands.h"
#include "song.h"
#include "libRemuxer.h"
#include "SongReader.h"
#include <libopenmpt/libopenmpt.hpp>
#include <libopenmpt/libopenmpt_ext.hpp>
#include <cstdint>
#include <memory>
#include <vector>

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
	int volSlideMem = 0;      // Axy / 5xy / 6xy volume-slide param memory
	int fineVolSlideMem = 0;  // EAx / EBx nibble memory
	int offsetMem = 0;        // CMD_OFFSET param memory
	Loop loop;
};

class ModReader : public SongReader
{
	// One audio render pass. channel < 0 => mixdown pass. channel >= 0 => render only that mod
	// channel to "<base>-chCC.wav": per-channel mode assigns it to track `midiTrack`; per-instrument
	// mode shares it among instrument tracks that play on the channel.
	struct Pass
	{
		int midiTrack; // whole-track WAV target (per-channel mode); unused (-1) for channel passes
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
