#include <string>
#include "libRemuxer.h"
#include "Song.h"
#include "ModReader.h"
#include "sidreader.h"

Marshal_Song marshalSong;
Song song(&marshalSong);
SongReader *songReader;
ModReader modReader(song);
SidReader sidReader(song);
UserArgs userArgs;

void initLib()
{
	ModReader::sInit();
	
	ZeroMemory(&marshalSong, sizeof(Marshal_Song));
	userArgs.inputPath = new char[MAX_PATH_LENGTH];
	userArgs.audioPath = new char[MAX_PATH_LENGTH];
	userArgs.midiPath = new char[MAX_PATH_LENGTH];

	marshalSong.tempoEvents = new Marshal_TempoEvent[MAX_TEMPOEVENTS];
	marshalSong.tracks = new Marshal_Track[MAX_MIDITRACKS];
	for (int t = 0; t < MAX_MIDITRACKS; t++)
	{
		ZeroMemory(&marshalSong.tracks[t], sizeof(Marshal_Track));
		marshalSong.tracks[t].name = new char[MAX_TRACKNAME_LENGTH];
		marshalSong.tracks[t].name[0] = 0;
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
	safeDeleteArray(userArgs.inputPath);
	safeDeleteArray(userArgs.audioPath);
	safeDeleteArray(userArgs.midiPath);
}

//const int MAX_EFFECTS_PER_CELL = 10;

BOOL beginProcessing(UserArgs &args)
{
	userArgs.insTrack = args.insTrack;
	userArgs.songLengthS = args.songLengthS;
	userArgs.subSong = args.subSong;
	if (args.inputPath)
		strcpy_s(userArgs.inputPath, MAX_PATH_LENGTH, args.inputPath);
	else
		userArgs.inputPath[0] = NULL;
	if (args.audioPath)
		strcpy_s(userArgs.audioPath, MAX_PATH_LENGTH, args.audioPath);
	else
		userArgs.audioPath[0] = NULL;
	if (args.midiPath)
		strcpy_s(userArgs.midiPath, MAX_PATH_LENGTH, args.midiPath);
	else
		userArgs.midiPath[0] = NULL;
	
	try 
	{
		modReader.beginProcessing(userArgs);
		song.marSong->songType = Marshal_SongType::Mod;
		songReader = &modReader;
	}
	catch (...)
	{
		try
		{
			sidReader.beginProcess(userArgs);
			args.subSong = userArgs.subSong;
			args.numSubSongs = userArgs.numSubSongs;
			song.marSong->songType = Marshal_SongType::Sid;
			songReader = &sidReader;
		}
		catch (...)
		{
			return FALSE;
		}
	}
			
	return TRUE;
}

float process()
{
	return songReader->process();
}

void finish()
{
	songReader->finish();
}




