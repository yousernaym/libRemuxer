#include <string>
#include "libRemuxer.h"
#include "Utf8Path.h"
#include "Song.h"
#include "ModReader.h"
#include "sidreader.h"
#include "HvlReader.h"

SongData songData;
Song song(&songData);
SongReader *songReader;
ModReader modReader(song);
SidReader sidReader(song);
HvlReader hvlReader(song);
UserArgs userArgs;

void initLib()
{
	ZeroMemory(&songData, sizeof(SongData));
	userArgs.inputPath = new char[MAX_PATH_LENGTH];
	userArgs.audioPath = new char[MAX_PATH_LENGTH];
	userArgs.midiPath = new char[MAX_PATH_LENGTH];
	userArgs.trackAudioBasePath = new char[MAX_PATH_LENGTH];

	songData.tempoEvents = new TempoEvent[MAX_TEMPOEVENTS];
	songData.tracks = new SongTrack[MAX_MIDITRACKS];
	for (int t = 0; t < MAX_MIDITRACKS; t++)
	{
		ZeroMemory(&songData.tracks[t], sizeof(SongTrack));
		songData.tracks[t].name = new char[MAX_TRACKNAME_LENGTH];
		songData.tracks[t].name[0] = 0;
		songData.tracks[t].notes = new SongNote[MAX_TRACKNOTES];
	}
}

#define safeDeleteArray(x) {if (x) delete[] x;}
void closeLib()
{
	safeDeleteArray(songData.tempoEvents);
	if (songData.tracks)
	{
		for (int t = 0; t < MAX_MIDITRACKS; t++)
		{
			safeDeleteArray(songData.tracks[t].name);
			safeDeleteArray(songData.tracks[t].notes);
		}
		safeDeleteArray(songData.tracks);
	}
	safeDeleteArray(userArgs.inputPath);
	safeDeleteArray(userArgs.audioPath);
	safeDeleteArray(userArgs.midiPath);
	safeDeleteArray(userArgs.trackAudioBasePath);
}

//const int MAX_EFFECTS_PER_CELL = 10;

BOOL beginProcessing(UserArgs &args)
{
	userArgs.insTrack = args.insTrack;
	userArgs.songLengthS = args.songLengthS;
	userArgs.subSong = args.subSong;
	// Reject oversize UTF-8 paths instead of strcpy_s truncating / aborting.
	if (!copyUtf8Path(userArgs.inputPath, MAX_PATH_LENGTH, args.inputPath)
		|| !copyUtf8Path(userArgs.audioPath, MAX_PATH_LENGTH, args.audioPath)
		|| !copyUtf8Path(userArgs.midiPath, MAX_PATH_LENGTH, args.midiPath)
		|| !copyUtf8Path(userArgs.trackAudioBasePath, MAX_PATH_LENGTH, args.trackAudioBasePath))
	{
		return FALSE;
	}

	try 
	{
		modReader.beginProcessing(userArgs);
		songReader = &modReader;
	}
	catch (...)
	{
		try
		{
			hvlReader.beginProcess(userArgs);
			args.subSong     = userArgs.subSong;
			args.numSubSongs = userArgs.numSubSongs;
			songReader = &hvlReader;
		}
		catch (...)
		{
			try
			{
				sidReader.beginProcess(userArgs);
				args.subSong = userArgs.subSong;
				args.numSubSongs = userArgs.numSubSongs;
				songReader = &sidReader;
			}
			catch (...)
			{
				return FALSE;
			}
		}
	}
			
	return TRUE;
}

float process()
{
	return songReader->process();
}

void endProcessing()
{
	songReader->endProcessing();
}

int getNumTrackAudioFiles()
{
	return songReader ? (int)songReader->getTrackAudioFiles().size() : 0;
}

BOOL getTrackAudioFile(int index, int *midiTrack, int *channel, char *path, int maxPathLength)
{
	if (!songReader)
		return FALSE;
	const auto &files = songReader->getTrackAudioFiles();
	if (index < 0 || index >= (int)files.size())
		return FALSE;
	if (midiTrack)
		*midiTrack = files[index].midiTrack;
	if (channel)
		*channel = files[index].channel;
	if (path && maxPathLength > 0)
	{
		if (!copyUtf8Path(path, (size_t)maxPathLength, files[index].path.c_str()))
			return FALSE;
	}
	return TRUE;
}




