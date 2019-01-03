#pragma once
#include "song.h"
#include <sidplayfp/sidplayfp.h>


namespace sid
{
	int main(Song &song, int argc, const char **argv, double songLengthS, int subSong);
};

class SidReader
{
	sidplayfp m_engine;
public:
	SidReader(Song &song, const char *path, double songLengthS, int subSong);
	~SidReader();
	char* loadRom(const char* path, size_t romSize);
	int initLSPfp(const char *path, int subSong);
	void process();
};

