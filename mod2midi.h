//New algorithm for reading mods and creating notes.
//Algorithm divided in two parts:
//1 - importMod
	//Create a list of tempoEvents like before.
	//Create one pattern for the whole song with a row for each tick.
	//Each tick contains information about the currently playing note's pitch and start time.
//2.createNoteList
	//Go through the pattern created in step 1 and create a note list for midi export.
//
//importMod could be replaced with readSid or readWhatever.

#include <mikmod.h>
#include <mikmod_internals.h>
#include <vector>
#include <iostream>
#include <math.h>
#include <set>
#include <assert.h>

using namespace std;

extern MODULE *module;

const int MAX_TEMPOEVENTS = 1000;
const int MAX_TRACKNOTES = 15000;
const int MAX_MIDITRACKS = 129; //128 (0x80) + global track. (Ft2 has a limit of 0x80 insturments, it should be enough for most mods even if other trackers allow more.
const int MAX_PITCHES = 128;
const int MAX_MODTRACKS = 64;

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
	__declspec(dllexport) BOOL initLib(char *_mixdownFilename);
	__declspec(dllexport) void exitLib();
	__declspec(dllexport) BOOL loadFile(char *path, Marshal_Song &mod, BOOL mixdown, BOOL insTrack);
	__declspec(dllexport) char *getModMixdownFilename_intptr();
}

//------------------------------------
//from mikmod_internal.h:

//enum {
//	/* Simple note */
//	UNI_NOTE = 1,
//	/* Instrument change */
//	UNI_INSTRUMENT,
//	/* Protracker effects */
//	UNI_PTEFFECT0,     /* arpeggio */
//	UNI_PTEFFECT1,     /* porta up */
//	UNI_PTEFFECT2,     /* porta down */
//	UNI_PTEFFECT3,     /* porta to note */
//	UNI_PTEFFECT4,     /* vibrato */
//	UNI_PTEFFECT5,     /* dual effect 3+A */
//	UNI_PTEFFECT6,     /* dual effect 4+A */
//	UNI_PTEFFECT7,     /* tremolo */
//	UNI_PTEFFECT8,     /* pan */
//	UNI_PTEFFECT9,     /* sample offset */
//	UNI_PTEFFECTA,     /* volume slide */
//	UNI_PTEFFECTB,     /* pattern jump */
//	UNI_PTEFFECTC,     /* set volume */
//	UNI_PTEFFECTD,     /* pattern break */
//	UNI_PTEFFECTE,     /* extended effects */
//	UNI_PTEFFECTF,     /* set speed */
//	/* Scream Tracker effects */
//	UNI_S3MEFFECTA,    /* set speed */
//	UNI_S3MEFFECTD,    /* volume slide */
//	UNI_S3MEFFECTE,    /* porta down */
//	UNI_S3MEFFECTF,    /* porta up */
//	UNI_S3MEFFECTI,    /* tremor */
//	UNI_S3MEFFECTQ,    /* retrig */
//	UNI_S3MEFFECTR,    /* tremolo */
//	UNI_S3MEFFECTT,    /* set tempo */
//	UNI_S3MEFFECTU,    /* fine vibrato */
//	UNI_KEYOFF,        /* note off */
//	/* Fast Tracker effects */
//	UNI_KEYFADE,       /* note fade */
//	UNI_VOLEFFECTS,    /* volume column effects */
//	UNI_XMEFFECT4,     /* vibrato */
//	UNI_XMEFFECT6,     /* dual effect 4+A */
//	UNI_XMEFFECTA,     /* volume slide */
//	UNI_XMEFFECTE1,    /* fine porta up */
//	UNI_XMEFFECTE2,    /* fine porta down */
//	UNI_XMEFFECTEA,    /* fine volume slide up */
//	UNI_XMEFFECTEB,    /* fine volume slide down */
//	UNI_XMEFFECTG,     /* set global volume */
//	UNI_XMEFFECTH,     /* global volume slide */
//	UNI_XMEFFECTL,     /* set envelope position */
//	UNI_XMEFFECTP,     /* pan slide */
//	UNI_XMEFFECTX1,    /* extra fine porta up */
//	UNI_XMEFFECTX2,    /* extra fine porta down */
//	/* Impulse Tracker effects */
//	UNI_ITEFFECTG,     /* porta to note */
//	UNI_ITEFFECTH,     /* vibrato */
//	UNI_ITEFFECTI,     /* tremor (xy not incremented) */
//	UNI_ITEFFECTM,     /* set channel volume */
//	UNI_ITEFFECTN,     /* slide / fineslide channel volume */
//	UNI_ITEFFECTP,     /* slide / fineslide channel panning */
//	UNI_ITEFFECTT,     /* slide tempo */
//	UNI_ITEFFECTU,     /* fine vibrato */
//	UNI_ITEFFECTW,     /* slide / fineslide global volume */
//	UNI_ITEFFECTY,     /* panbrello */
//	UNI_ITEFFECTZ,     /* resonant filters */
//	UNI_ITEFFECTS0,
//	/* UltraTracker effects */
//	UNI_ULTEFFECT9,    /* Sample fine offset */
//	/* OctaMED effects */
//	UNI_MEDSPEED,
//	UNI_MEDEFFECTF1,   /* play note twice */
//	UNI_MEDEFFECTF2,   /* delay note */
//	UNI_MEDEFFECTF3,   /* play note three times */
//	/* Oktalyzer effects */
//	UNI_OKTARP,		   /* arpeggio */
//
//	UNI_LAST
//};
//----------------------------------
//extern UWORD unioperands[UNI_LAST];

const int MAX_EFFECT_VALUES = 2;

struct RunningCellInfo
{
	int ins;
	int note;
	//int silentNote;
	int noteStartT;
	double noteStartS;
	//int volEnvStartT;
	double volEnvStartS;
	//int sampleStartT;
	double sampleStartS;
	bool loopSample;
	double sampleLength;
	int volEnvEnd;
	int startVol;
	//int vol;
	bool samplePlaying;
	bool volEnvEnded;
	int repeatsLeft;
	int offset;
	//int volVelocity;
	//int volVelScale;
	//int noteStartOffset;
	//int noteEndOffset;
	int effValues[UNI_LAST][MAX_EFFECT_VALUES];
};


typedef multiset<Marshal_Note> TrackNotes;
//typedef vector<TrackNotes> Notes;

struct RunningTickInfo
{
	int noteStart = -1;
	int notePitch;
	int vol;
	int ins;
};

struct Track
{
	vector<RunningTickInfo> ticks;
	RunningTickInfo getPrevTick(int curTick)
	{
		return ticks[curTick > 0 ? --curTick : 0];
	}
};

class Song
{
	vector<Track> tracks;
	vector<RunningCellInfo> runningRowInfo;
	vector<TrackNotes> notes;
public:
	BOOL importMod(char *path, Marshal_Song &marSong, BOOL mixdown, BOOL insTrack);
	BOOL createNoteList(Marshal_Song &marSong, BOOL insTrack);
};

void freeMod();
void getCellRepLen(BYTE replen, int &repeat, int &length);
bool addNote(vector<RunningCellInfo> &row, vector<TrackNotes> &notes, bool modInsTrack, int channel, int noteEnd);
double getRowDur(double tempo, double speed);
