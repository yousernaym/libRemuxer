#pragma once
#include <set>
#include <vector>
#include "NoteExtractor.h"
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
	vector<TrackNotes> notes;
public:
	vector<Track> tracks;
	Marshal_Song *marSong;
	Song(Marshal_Song *_marSong);
	BOOL createNoteList(BOOL insTrack);
};
