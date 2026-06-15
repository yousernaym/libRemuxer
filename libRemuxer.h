#pragma once
#include <mikmod.h>
#include <mikmod_internals.h>
#include <cmath>

const int MAX_PATH_LENGTH = 256;

const int MAX_TEMPOEVENTS = 10000;
const int MAX_TRACKNOTES = 150000;
const int MAX_MIDITRACKS = 129; //128 (0x80) + global track. (Ft2 has a limit of 0x80 insturments, it should be enough for most mods even if other trackers allow more.
const int MAX_MODTRACKS = 64;
const int MAX_TRACKNAME_LENGTH = 100;

struct TempoEvent
{
	int time;
	double tempo;
};

struct SongNote
{
	int start;
	int stop;
	int pitch;
	int chn;
	bool operator<(const SongNote &right) const
	{
		return start < right.start;
	}
};

struct SongTrack
{
	char *name;
	SongNote *notes;
	int numNotes;
};

struct SongData
{
	TempoEvent *tempoEvents;
	int numTempoEvents;
	int ticksPerBeat; 
	SongTrack *tracks;
	int numTracks;
};

#pragma pack(push, 8)
struct UserArgs
{
	char *inputPath;
	char *audioPath;
	char *midiPath;
	BOOL insTrack;
	float songLengthS;
	int subSong;
	int numSubSongs;
};
#pragma pack(pop, 8)

extern "C"
{
	__declspec(dllexport) void initLib();
	__declspec(dllexport) void closeLib();
	__declspec(dllexport) BOOL beginProcessing(UserArgs &a);
	__declspec(dllexport) float process();
	__declspec(dllexport) void endProcessing();
}
