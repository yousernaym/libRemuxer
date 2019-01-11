#pragma once
#include "song.h"
#include "remuxer.h"
#include <sidplayfp/sidplayfp.h>

class SidReader
{
	sidplayfp m_engine;
public:
	SidReader(Song &song);
	~SidReader();
	char* loadRom(const char* path, size_t romSize);
	int process();
};

