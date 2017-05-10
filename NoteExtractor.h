#pragma once
#include <mikmod.h>
#include <mikmod_internals.h>
#include <math.h>

using namespace std;


extern char mixdownFilename[500];

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
};
#pragma pack(pop)

extern "C"
{
	__declspec(dllexport) void initLib(char *_mixdownFilename);
	__declspec(dllexport) void exitLib();
	__declspec(dllexport) BOOL loadFile(char *path, Marshal_Song &mod, BOOL mixdown, BOOL insTrack);
	__declspec(dllexport) char *getModMixdownFilename_intptr();
}


//void freeMod();
