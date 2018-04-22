#pragma once
#include "song.h"

namespace sid
{
	int main(Song &song, int argc, const char **argv, double songLengthS);
};

class SidReader
{
public:
	SidReader(Song &song, const char *path, double songLengthS);
	~SidReader();
};

