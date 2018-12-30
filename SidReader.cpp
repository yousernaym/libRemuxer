#include "SidReader.h"
#include "sidspectro\\sidfile.h"
#include "sidspectro\\c64.h"


SidReader::SidReader(Song &song, const char *path, double songLengthS, int subSong)
{
	//sid::main(song, 1, &path, songLengthS, subSong);
	int fps = 60;
	song.marSong->ticksPerBeat = 24;
	float bpm = fps * 60 / song.marSong->ticksPerBeat;
	song.marSong->tempoEvents[0].tempo = bpm;
	song.marSong->tempoEvents[0].time = 0;
	song.marSong->numTempoEvents = 1;
	song.tracks.resize(3);
	for (int i = 0; i < 3; i++)
	{
		song.tracks[i].ticks.resize(int(songLengthS * fps) + 1);
		sprintf_s(song.marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, "Channel %i", i + 1);
	}

	SIDFile sidFile(path);
	if (subSong > 0)
		sidFile.startsong = subSong;

	C64 c64;
	c64.LoadSIDFile(sidFile);
	return;
	float timeS = 0;
	float ticksPerSeconds = bpm / 60 * song.marSong->ticksPerBeat;
	while (timeS < songLengthS)
	{
		c64.MainLoop();
		int timeT = (int)(timeS * ticksPerSeconds);
		for (int i = 0; i < 3; i++)
		{
			song.tracks[i].ticks[timeT].noteStart = song.tracks[i].getPrevTick(timeT)->noteStart;
			SIDChannel ch = c64.sid.channel[i];
			if (ch.isPlaying())
			{
				song.tracks[i].ticks[timeT].notePitch =  (int)(log2(ch.frequency / 20) * 12);
				if (song.tracks[i].getPrevTick(timeT)->vol == 0)
					song.tracks[i].ticks[timeT].noteStart = timeT;
				song.tracks[i].ticks[timeT].vol = ch.amplitude;
			}
		}
		timeS = c64.getTime();
	}
}


SidReader::~SidReader()
{
}
