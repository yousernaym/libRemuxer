#pragma once
#include <mikmod.h>
#include <mikmod_internals.h>
#include <math.h>

using namespace std;

const int MAX_MIXDOWN_PATH_LENGTH = 500;
extern char mixdownPath[MAX_MIXDOWN_PATH_LENGTH];

const int MAX_TEMPOEVENTS = 1000;
const int MAX_TRACKNOTES = 15000;
const int MAX_MIDITRACKS = 129; //128 (0x80) + global track. (Ft2 has a limit of 0x80 insturments, it should be enough for most mods even if other trackers allow more.
const int MAX_PITCHES = 128;
const int MAX_MODTRACKS = 64;
const int MAX_TRACKNAME_LENGTH = 100;

#pragma pack(push, 8)
struct Marshal_TempoEvent
{
	int time;
	double tempo;
};

struct Marshal_Note
{
	int start;
	int stop;
	int pitch;
	bool operator<(const Marshal_Note &right) const
	{
		return start < right.start;
	}
};

struct Marshal_Track
{
	char *name;
	Marshal_Note *notes;
	int numNotes;
};

enum class Marshal_SongType {Mod, Sid};

struct Marshal_Song
{
	Marshal_TempoEvent *tempoEvents;
	int numTempoEvents;
	//int ticksPerMeasure; //"timeDiv" from midi song class
	int songLengthT;
	int minPitch;
	int maxPitch;
	Marshal_Track *tracks;
	int numTracks;
	Marshal_SongType songType;
};
#pragma pack(pop)

extern "C"
{
	__declspec(dllexport) void initLib();
	__declspec(dllexport) void exitLib();
	__declspec(dllexport) BOOL loadFile(const char *path, Marshal_Song &mod, const char *mixdownPath, BOOL insTrack, double songLengthS);
	__declspec(dllexport) char *getMixdownPath();
}


//void freeMod();
