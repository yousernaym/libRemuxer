#pragma once
#include <mikmod.h>
#include <mikmod_internals.h>
#include "song.h"

using namespace std;
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


class ModReader
{
	static int curSongSpeed;
	static int ptnDelay;
	static Song *song;
	static Marshal_Song *marSong;
public:
	static void init();
	static BOOL loadFile(Song &_song, char *path, BOOL mixdown, BOOL insTrack);
	static void getCellRepLen(BYTE replen, int &repeat, int &length);
	//bool addNote(vector<RunningCellInfo> &row, vector<TrackNotes> &notes, bool modInsTrack, int channel, int noteEnd);
	static double getRowDur(double tempo, double speed);
	static void readRowFx();

	/*ModReader();
	~ModReader();*/
};

