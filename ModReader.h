#pragma once
#include <mikmod.h>
#include <mikmod_internals.h>
#include "song.h"
#include "libRemuxer.h"
#include "SongReader.h"
#include <libopenmpt/libopenmpt.hpp>

const int MAX_EFFECT_VALUES = 2;

struct Loop
{
	int startR = 0;
	int loops = 0;
};


struct CellInfo
{
	int ins = 0;
	int note = 0;
	int eff[UNI_LAST];
	int effValues[UNI_LAST][MAX_EFFECT_VALUES];
	int numEffs = 0;
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
	int ins;
	int note;
	int noteStartT;
	double noteStartS;
	double volEnvStartS;
	double sampleStartS;
	bool loopSample;
	double sampleLength;
	int volEnvEnd;
	int startVol;
	bool samplePlaying;
	bool volEnvEnded;
	int repeatsLeft;
	int offset;
	int effValues[UNI_LAST][MAX_EFFECT_VALUES];
	Loop loop;
};

class ModReader : public SongReader
{
	const double semitone = 1.0594630943593; //12th root of 2
	const int FadeOutTimeS = 7;
	std::vector<RunningCellInfo> runningRowInfo;
	std::vector<CellInfo> curRowInfo;
	MODULE *module = 0;
	std::unique_ptr<openmpt::module> omptModule;
	int curSongSpeed;
	int ptnDelay;
	int timeT = 0;
	double timeS = 0;
	double rowDur = 0;
	double tickDur = 0;
	int ptnJump = -1;
	int ptnStart = 0;
	//Loop loop;
	Marshal_Song *marSong;
	bool isFadingOut;
public:
	ModReader(Song &song);
	~ModReader();
	static void sInit();
	void getCellRepLen(BYTE replen, int &repeat, int &length);
	double getRowDur(double tempo, double speed);
	void readNextCell(BYTE *track, CellInfo &cellInfo, RunningCellInfo &runningCellInfo);
	bool readCellFx(RunningTickInfo &firstTick, CellInfo &cellInfo, RunningCellInfo &runningCellInfo, int songPos, int row);
	void updateCell(RunningTickInfo &firstTick, const CellInfo &cellInfo, RunningCellInfo &runningCellInfo);
	void updateCellTicks(Song::Track &track, const CellInfo &cellInfo, RunningCellInfo &runningCellInfo);
	void extractNotes();
	void beginProcessing(const UserArgs &args);
	float process() override;
	void finish() override;
};

