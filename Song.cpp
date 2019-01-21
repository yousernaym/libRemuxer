#include "song.h"
#include "fstream"
#include <assert.h>

Song::Song(Marshal_Song *_marSong)
{
	marSong = _marSong;
}

void Song::createNoteList(const Args &args, const std::set<int> *usedInstruments)
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
				note.chn = i % 16; //Keep channel number below 16 for midi compatibility. //TODO: Keep track of which channels are used and choose unused ones, to avoid midi notes on the same channel and same pitch cutting each other off.
				unsigned track;
				if (args.insTrack)
				{
					int noteIns = prevTick.ins;
					if (usedInstruments)
					{
						//Only create tracks for instruments that were actually used
						int t = 1;
						for each (int ins in *usedInstruments)
						{
							if (ins == noteIns)
								track = t;
							t++;
						}
					}
					else
					{
						//Track number corresponds with instrument number
						track = noteIns; //No need for +1 because instruments start at 1
					}
				}
				else
				{
					//Track number corresponds with channel number + 1 since track 0 should only be used for tempo and such
					track = i + 1; 
				}
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

	if (args.midiPath[0])
		saveMidiFile(args.midiPath);
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
			//Time signature
			writeVL(0);  //Delta time
			writeBE(3, 0xff5804); //Event type = time sig
			writeBE(4, 0x04022408); //Event type = time sig

			for (int i = 0; i < marSong->numTempoEvents; i++)
			{
				writeVL(marSong->tempoEvents[i].time - absoluteTime);
				absoluteTime = marSong->tempoEvents[i].time;
				writeBE(2, 0Xff51); //Event type = tempo
				writeVL(3); //Length of event data
				writeBE(3, unsigned(60000000 / marSong->tempoEvents[i].tempo)); //Microseconds per beat
			}
		}
		else //Track 1+
		{
			Marshal_Track track = marSong->tracks[t];
			writeVL(0); //Time
			writeBE(2, 0xff03); //Track name event type
			writeVL((unsigned)strlen(track.name)); //Length of track name
			outFile << track.name;
			map<int, vector<MidiNoteEvent>> noteEvents;
			createNoteEvents(&noteEvents, track);
			for each (const auto &eventsAtTime in noteEvents)
			{
				for each (const auto &noteEvent in eventsAtTime.second)
				{
					writeVL(eventsAtTime.first - absoluteTime);
					absoluteTime = eventsAtTime.first;
					writeByte((noteEvent.on ? 0x90 : 0x80) | noteEvent.chn); //note on/off
					writeByte(noteEvent.pitch); //pitch
					writeByte(64); //velocity
				}
			}
		}
		writeVL(0); //End of track event, at same time as last event
		writeBE(3, 0xff2f00); //End of track event
		
		//Go back and write size of track chunk
		std::streampos endOfTrackPos = outFile.tellp();
		outFile.seekp(trackLengthPos);
		writeBE(4, (unsigned)(endOfTrackPos - trackLengthPos) - 4);
		outFile.seekp(endOfTrackPos);
	}
	outFile.close();
}

void Song::writeVL(unsigned value)
{
	int numBytes = 0;
	int buffer = 0;
	//value = 0xffff;
	do
	{
		buffer |= ((value & 0x7f) | (0x80 * (numBytes > 0))) << (8 * numBytes++);
	} while (value >>= 7);
	writeBE(numBytes, buffer);
}

void Song::createNoteEvents(map<int, vector<MidiNoteEvent>> *noteEvents, Marshal_Track track)
{
	for (int i = 0; i < track.numNotes; i++)
	{
		auto note = track.notes[i];
		MidiNoteEvent noteEvent;
		noteEvent.on = true;
		noteEvent.pitch = note.pitch;
		noteEvent.chn = note.chn;
		(*noteEvents)[note.start].push_back(noteEvent);
		noteEvent.on = false;
		(*noteEvents)[note.stop].push_back(noteEvent);
	}
}

