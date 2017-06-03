#pragma once
#include "song.h"

class SidReader
{
public:
	SidReader(Song &song, const char *path, double songLengthS);
	~SidReader();
};

