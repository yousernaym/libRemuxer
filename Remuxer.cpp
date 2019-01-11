#include <string>
#include "Remuxer.h"
#include "Song.h"
#include "ModReader.h"
#include "sidreader.h"

Marshal_Song marshalSong;
Args g_args;

void initLib()
{
	ModReader::sInit();
	
	ZeroMemory(&marshalSong, sizeof(Marshal_Song));
	g_args.inputPath = new char[MAX_PATH_LENGTH];
	g_args.audioPath = new char[MAX_PATH_LENGTH];
	g_args.midiPath = new char[MAX_PATH_LENGTH];

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
}

#define safeDeleteArray(x) {if (x) delete[] x;}
void closeLib()
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
	safeDeleteArray(g_args.inputPath);
	safeDeleteArray(g_args.audioPath);
	safeDeleteArray(g_args.midiPath);
}

//const int MAX_EFFECTS_PER_CELL = 10;

BOOL beginProcessing(Args &a)
{
	g_args.insTrack = a.insTrack;
	g_args.songLengthS = a.songLengthS;
	g_args.subSong = a.subSong;
	Song song(&marshalSong);
	if (a.inputPath != NULL)
		strcpy_s(g_args.inputPath, MAX_PATH_LENGTH, a.inputPath);
	else
		g_args.inputPath[0] = NULL;
	if (a.audioPath != NULL)
		strcpy_s(g_args.audioPath, MAX_PATH_LENGTH, a.audioPath);
	else
		g_args.audioPath[0] = NULL;
	if (a.midiPath != NULL)
		strcpy_s(g_args.midiPath, MAX_PATH_LENGTH, a.midiPath);
	else
		g_args.midiPath[0] = NULL;
	
	try 
	{
		ModReader modReader(song);
		song.marSong->songType = Marshal_SongType::Mod;
	}
	catch (...)
	{
		try
		{
			SidReader sidReader(song); 
			song.marSong->songType = Marshal_SongType::Sid;
		}
		catch (...)
		{
			return FALSE;
		}
	}
			
	song.createNoteList();
	if (g_args.midiPath[0])
		song.saveMidiFile(g_args.midiPath);
	return TRUE;
}

float process()
{
	return 1;
}



