#pragma once
#include <set>
#include <vector>
#include <map>
#include "Remuxer.h"
#include "FileFormat.h"
typedef multiset<Marshal_Note> TrackNotes;
//typedef vector<TrackNotes> Notes;

struct RunningTickInfo
{
	int noteStart = -1;
	int notePitch = 0;
	int vol = 0;
	int ins = 0;
};

struct MidiNoteEvent
{
	bool on;
	int pitch;
	int chn;
};

class Song : FileFormat
{
private:
	vector<TrackNotes> notes;
	void writeVL(unsigned value); //Write variable length value to file
	void createNoteEvents(map<int, vector<MidiNoteEvent>> *noteEvents, Marshal_Track track);
public:
	struct Track;
	vector<Track> tracks;
	Marshal_Song *marSong;
	Song(Marshal_Song *_marSong);
	void createNoteList(const Args &args);
	void saveMidiFile(const string &path);
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
