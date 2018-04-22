#include "SidReader.h"

SidReader::SidReader(Song &song, const char *path, double songLengthS)
{
	sid::main(song, 1, &path, songLengthS);
}


SidReader::~SidReader()
{
}
