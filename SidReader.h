#pragma once
#include "song.h"

class SidReader
{
public:
	SidReader(Song &song, char *path, BOOL mixdown);
	~SidReader();
};

