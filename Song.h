#pragma once
#include <set>
#include <vector>
#include "NoteExtractor.h"
typedef multiset<Marshal_Note> TrackNotes;
//typedef vector<TrackNotes> Notes;

struct RunningTickInfo
{
	int noteStart = -1;
	int notePitch = 0;
	int vol = 0;
	int ins = 0;
};

class Song
{
	vector<TrackNotes> notes;
public:
	struct Track;
	vector<Track> tracks;
	Marshal_Song *marSong;
	Song(Marshal_Song *_marSong);
	void createNoteList(BOOL insTrack);
};

struct Song::Track
{
	vector<RunningTickInfo> ticks;
	RunningTickInfo *getPrevTick(unsigned curTick)
	{
		return &ticks[curTick > 0 ? curTick - 1 : curTick];
	}
	RunningTickInfo *getNextTick(unsigned curTick)
	{
		return &ticks[curTick < ticks.size() - 1 ? curTick + 1 : curTick];
	}
};
