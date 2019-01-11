#include "song.h"
#include "fstream"
#include <assert.h>

Song::Song(Marshal_Song *_marSong)
{
	marSong = _marSong;
}

void Song::createNoteList()
{
	int resolutionScale = 480 / marSong->ticksPerBeat;
	marSong->ticksPerBeat = marSong->ticksPerBeat * resolutionScale;;
	marSong->minPitch = MAX_PITCHES;
	marSong->maxPitch = 0;
	notes.reserve(MAX_MIDITRACKS);
	marSong->songLengthT = (int)tracks[1].ticks.size() * resolutionScale;
	for (unsigned i = 0; i < tracks.size(); i++)
	{
		for (unsigned j = 0; j < tracks[i].ticks.size(); j++)
		{
			RunningTickInfo &curTick = tracks[i].ticks[j];
			RunningTickInfo &prevTick = *tracks[i].getPrevTick(j);
			
			//Add new note?
			if (prevTick.notePitch >= 0 &&
				prevTick.noteStart >= 0 &&
				prevTick.vol > 0 &&
				(curTick.noteStart != prevTick.noteStart ||
					curTick.vol == 0 ||
						j == tracks[i].ticks.size() - 1))
			{
				Marshal_Note note;
				note.pitch = prevTick.notePitch;
				note.start = int(prevTick.noteStart * resolutionScale);
				note.stop = int(j * resolutionScale);
				unsigned track = g_args.insTrack ? prevTick.ins : i + 1; //add 1 to channel because first track is reserved for "global" (modules can't have ins indices <1
				if (track >= notes.size())
					notes.resize(track + 1);
				notes[track].insert(note);
				if (notes[track].size() >= MAX_TRACKNOTES)
					throw (string)"Too many notes in track";

				if (marSong->maxPitch < note.pitch)
					marSong->maxPitch = note.pitch;
				if (marSong->minPitch > note.pitch)
					marSong->minPitch = note.pitch;
			}

		}
	}
	//Copy notes to marshal struct
	for (unsigned t = 0; t < notes.size(); t++)
	{
		marSong->numTracks = (int)notes.size();
		marSong->tracks[t].numNotes = (int)notes[t].size();
		int i = 0;
		for (TrackNotes::iterator it = notes[t].begin(); it != notes[t].end(); it++)
			marSong->tracks[t].notes[i++] = *it;
	}
	for (int i = 0; i < marSong->numTempoEvents; i++)
	{
		marSong->tempoEvents[i].time *= resolutionScale;
	}
}

void Song::saveMidiFile(const string &path)
{
	createFile(path);
	outFile << "MThd";
	writeBE(4, 6); //Length of header data
	writeBE(2, 1); //Midi format 1
	writeBE(2, marSong->numTracks);
	writeBE(2, marSong->ticksPerBeat); //MSB is 0,so use ticks per beat (quarter note)
	
	for (int t = 0; t < marSong->numTracks; t++)
	{
		int absoluteTime = 0;
		outFile << "MTrk";
		std::streampos trackLengthPos = outFile.tellp(); //Store file poInter for writing of track length
		writeBE(4, 0); //Write length as 0 for now
		if (t == 0) //Write tempo events to track 0
		{
			for (int i = 0; i < marSong->numTempoEvents; i++)
			{
				writeVL(marSong->tempoEvents[i].time);
				writeBE(2, 0Xff51); //Tempo event type
				writeVL(3); //Length of event data
				writeBE(3, 60000000 / marSong->tempoEvents[i].tempo); //Microseconds per beat
			}
		}
		else //Track 1+
		{
			Marshal_Track track = marSong->tracks[t];
			writeVL(0); //Time
			writeBE(2, 0xff03); //Track name event type
			writeVL(strlen(track.name)); //Length of track name
			outFile << track.name;
			map<int, vector<bool>> noteEvents;
			createNoteEvents(&noteEvents, track);
			for each (auto eventsAtTime in noteEvents)
			{
				int deltaTime = eventsAtTime.first - absoluteTime;
				for each (bool on in eventsAtTime.second)
				{
					writeVL(deltaTime);
					int status = on ? 0x90 : 0x80;
					writeByte(status); //note on/off
					writeByte(0x40); //velocity
				}
			}
		}
		writeBE(3, 0xff2f00); //End of track event
		std::streampos endOfTrackPos = outFile.tellp();
		outFile.seekp(trackLengthPos);
		writeBE(4, endOfTrackPos - trackLengthPos);
		outFile.seekp(endOfTrackPos);
	}
}

void Song::writeVL(unsigned value)
{
	int shift = 25;
	for (int i = 0; i < 4; i++)
	{
		int byte = (value >> shift) & 0x7f;
		if (i < 3)
			byte |= 0x80;
		writeByte(byte);		
	}
}

void Song::createNoteEvents(map<int, vector<bool>> *noteEvents, Marshal_Track track)
{
	for (int i = 0; i < track.numNotes; i++)
	{
		auto note = track.notes[i];
		(*noteEvents)[note.start].push_back(true);
	}
}

