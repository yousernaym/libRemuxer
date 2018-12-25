#include "SidReader.h"
#include "sidspectro\\sidfile.h"
#include "sidspectro\\c64.h"


SidReader::SidReader(Song &song, const char *path, double songLengthS, int subSong)
{
	//sid::main(song, 1, &path, songLengthS, subSong);
	int fps = 60;
	song.marSong->ticksPerBeat = 24;
	song.marSong->tempoEvents[0].tempo = fps * 60 / song.marSong->ticksPerBeat;
	song.marSong->tempoEvents[0].time = 0;
	song.marSong->numTempoEvents = 1;
	song.tracks.resize(3);
	for (int c = 0; c < 3; c++)
	{
		song.tracks[c].ticks.resize(int(songLengthS * fps) + 1);
		sprintf_s(song.marSong->tracks[c + 1].name, MAX_TRACKNAME_LENGTH, "Channel %i", c + 1);
	}

	SIDFile sidFile(path);
	if (subSong > 0)
		sidFile.startsong = subSong;
	C64 c64;
	c64.LoadSIDFile(sidFile);
	while (c64.getTime() < songLengthS)
	{
		c64.MainLoop();
	}
}


SidReader::~SidReader()
{
}
