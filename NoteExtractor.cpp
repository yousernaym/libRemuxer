#include <string>
#include "NoteExtractor.h"
#include "Song.h"
#include "ModReader.h"
#include "sidreader.h"
//#define BOOST_FILESYSTEM_NO_DEPRECATED
//#include <boost/filesystem.hpp>

//#pragma warning(disable : 4996)

Marshal_Song marshalSong;
char mixdownPath[MAX_MIXDOWN_PATH_LENGTH];


char *getMixdownPath()
{
	return mixdownPath;
}

void initLib()
{
	ModReader::sInit();
	//strcpy_s<MAX_MIXDOWN_PATH_LENGTH>(mixdownPath, _mixdownPath);
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
	//remove(mixdownPath);
}

//const int MAX_EFFECTS_PER_CELL = 10;

BOOL loadFile(const char *path, Marshal_Song &marSong, const char *_mixdownPath, BOOL insTrack, double songLengthS)
{
	marSong = marshalSong;
	Song song(&marSong);
	if (_mixdownPath != NULL)
		strcpy_s(mixdownPath, _mixdownPath);
	else
		mixdownPath[0] = NULL;

	//Determine mixdown path in caller instead
#pragma region Determine_mixdown_path
	/*string expandedPath(_mixdownPath);
	string nameMacro = "%notefilename";
	size_t macroIndex = expandedPath.find(nameMacro);
	if (macroIndex != string::npos)
	{
		string noteFilename = boost::filesystem::path(path).filename().string();
		expandedPath.replace(macroIndex, nameMacro.size(), noteFilename);
		strcpy_s(mixdownPath, expandedPath.c_str());
	}	*/
#pragma endregion
	
	try 
	{
		ModReader modReader(song, path, mixdownPath, insTrack);
		song.marSong->songType = Marshal_SongType::Mod;
	}
	catch (string s)
	{
		try
		{
			SidReader sidReader(song, path, songLengthS); 
			song.marSong->songType = Marshal_SongType::Sid;
		}
		catch (...)
		{
			return FALSE;
		}
	}
			
	song.createNoteList(insTrack);
	return TRUE;
}



