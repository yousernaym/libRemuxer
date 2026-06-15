#pragma once
#include <set>
#include <vector>
#include <map>
#include "libRemuxer.h"
#include "FileFormat.h"
typedef std::multiset<SongNote> TrackNotes;
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
	std::vector<TrackNotes> notes;
	void writeVL(unsigned value); //Write variable length value to file
	void createNoteEvents(std::map<int, std::vector<MidiNoteEvent>> *noteEvents, SongTrack track);
public:
	struct Track;
	std::vector<Track> tracks;
	SongData *songData;
	Song(SongData *_songData);
	void createNoteList(const UserArgs &args, const std::set<int> *usedInstruments = nullptr);
	void saveMidiFile(const std::string &path);
};

struct Song::Track
{
	std::vector<RunningTickInfo> ticks;
	RunningTickInfo *getPrevTick(unsigned curTick)
	{
		return &ticks[curTick > 0 ? curTick - 1 : curTick];
	}
	RunningTickInfo *getNextTick(unsigned curTick)
	{
		return &ticks[curTick < ticks.size() - 1 ? curTick + 1 : curTick];
	}
};
