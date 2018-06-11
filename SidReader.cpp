#include "SidReader.h"

SidReader::SidReader(Song &song, const char *path, double songLengthS, int subSong)
{
	sid::main(song, 1, &path, songLengthS, subSong);
}


SidReader::~SidReader()
{
}
