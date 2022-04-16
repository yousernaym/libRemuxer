#pragma once

//Needed for sidplayfp
#define HAVE_CXX14
#define HAVE_CXX11

#include "song.h"
#include "libRemuxer.h"
#include "SongReader.h"
#include "Wav.h"
#include <sidplayfp/sidplayfp.h>
#include <builders/residfp-builder/residfp.h> 

class SidReader : public SongReader
{
	sidplayfp engine;
	std::unique_ptr<ReSIDfpBuilder> rs;
	NoteState noteState;
	int minFreq;
	int maxFreq;
	float timeS;
	float ticksPerSeconds;
	int oldTimeT;
	int samplesProcessed;
	int samplesToProcess;
	int samplesBeforeFadeout;
public:
	SidReader(Song &song);
	~SidReader();
	std::vector<char> loadRom(const char* path, size_t romSize);
	void beginProcess(UserArgs &_args);
	float process() override;
	void endProcessing() override;
};

