#pragma once

#include "Song.h"

class SongReader
{
protected:
	Song &song;
public:
	SongReader(Song &_song);
	virtual ~SongReader();
	virtual float process() = 0;
};

