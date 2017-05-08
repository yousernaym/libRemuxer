#include "song.h"

Song::Song(Marshal_Song *_marSong)
{
	marSong = _marSong;
}

BOOL Song::createNoteList(BOOL insTrack)
{
	notes.reserve(MAX_MIDITRACKS);

	for (unsigned i = 0; i < tracks.size(); i++)
	{
		for (unsigned j = 0; j < tracks[i].ticks.size(); j++)
		{
			RunningTickInfo curTick = tracks[i].ticks[j];
			RunningTickInfo prevTick = tracks[i].getPrevTick(j);
			//Add new note?
			if (prevTick.notePitch >= 0 &&
				prevTick.noteStart >= 0 &&
				(curTick.noteStart != prevTick.noteStart ||
					prevTick.vol > 0 && (curTick.vol == 0 ||
						j == tracks[i].ticks.size() - 1)))
			{
				Marshal_Note note;
				note.pitch = prevTick.notePitch;
				note.start = prevTick.noteStart;
				note.stop = j;
				unsigned track = insTrack ? prevTick.ins : i + 1; //add 1 to channel because first track is reserved for "global" (modules can't have ins indices <1
				if (track >= notes.size())
					notes.resize(track + 1);
				notes[track].insert(note);
				if (notes[track].size() >= MAX_TRACKNOTES)
					return FALSE;

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
		marSong->numTracks = notes.size();
		marSong->tracks[t].numNotes = notes[t].size();
		int i = 0;
		for (TrackNotes::iterator it = notes[t].begin(); it != notes[t].end(); it++)
			marSong->tracks[t].notes[i++] = *it;
	}
	return TRUE;
}
