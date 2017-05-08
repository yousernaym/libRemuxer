//#include <sstream>
#include <string>
#include "NoteExtractor.h"
#include "Song.h"
#include "ModReader.h"

//#pragma warning(disable : 4996)

Marshal_Song marshalSong;
//Notes notes;
const int MAX_MIXDOWN_FILENAME = 500;
char mixdownFilename[MAX_MIXDOWN_FILENAME];

char *getModMixdownFilename_intptr()
{
	return mixdownFilename;
}

BOOL initLib(char *_mixdownFilename)
{
	ModReader::init();
	strcpy_s(mixdownFilename, MAX_MIXDOWN_FILENAME, _mixdownFilename);
	ZeroMemory(&marshalSong, sizeof(Marshal_Song));
    
	marshalSong.tempoEvents = new Marshal_TempoEvent[MAX_TEMPOEVENTS];
	marshalSong.tracks = new Marshal_Track[MAX_MIDITRACKS];
	for (int t = 0; t < MAX_MIDITRACKS; t++)
	{
		ZeroMemory(&marshalSong.tracks[t], sizeof(Marshal_Track));
		marshalSong.tracks[t].name = new char[MAX_TRACKNAME_LENGTH];
		marshalSong.tracks[t].notes = new Marshal_Note[MAX_TRACKNOTES];
		//for (int n = 0; n < MAX_MIDITRACKS; n++)
			//ZeroMemory(&marshalModule.tracks[t].notes[n], sizeof(Marshal_Note));
	}
	
	return TRUE;
}

#define safeDeleteArray(x) {if (x) delete[] x;}
void exitLib()
{
	safeDeleteArray(marshalSong.tempoEvents);
	if (marshalSong.tracks)
	{
		for (int t = 0; t < MAX_MIDITRACKS; t++)
		{
			safeDeleteArray(marshalSong.tracks[t].name);
			safeDeleteArray(marshalSong.tracks[t].notes);
		}
		safeDeleteArray(marshalSong.tracks);
	}
	remove(mixdownFilename);
}

//const int MAX_EFFECTS_PER_CELL = 10;

BOOL loadFile(char *path, Marshal_Song &marSong, BOOL mixdown, BOOL insTrack)
{
	marSong = marshalSong;
	Song song(&marSong);
	if (!ModReader::loadFile(song, path, mixdown, insTrack))
		return FALSE;
	if (!song.createNoteList(insTrack))
		return FALSE;
	return TRUE;
}


