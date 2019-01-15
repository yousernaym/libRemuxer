#pragma once
#include <mikmod.h>
#include <mikmod_internals.h>
#include <cmath>

using namespace std;


const int MAX_PATH_LENGTH = 256;

const int MAX_TEMPOEVENTS = 10000;
const int MAX_TRACKNOTES = 150000;
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
	int ticksPerBeat; 
	int songLengthT;
	int minPitch;
	int maxPitch;
	Marshal_Track *tracks;
	int numTracks;
	Marshal_SongType songType;
};

struct Args
{
	char *inputPath;
	char *audioPath;
	char *midiPath;
	BOOL insTrack;
	float songLengthS;
	int subSong;
};
#pragma pack(pop, 8)

extern "C"
{
	__declspec(dllexport) void initLib();
	__declspec(dllexport) void closeLib();
	__declspec(dllexport) BOOL beginProcessing(Args &a);
	__declspec(dllexport) float process();
	__declspec(dllexport) void finish();
}