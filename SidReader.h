#pragma once
#include "song.h"
#include "remuxer.h"
#include "SongReader.h"
#include "Wav.h"
#include <sidplayfp/sidplayfp.h>
#include <builders/residfp-builder/residfp.h> 

class SidReader : public SongReader
{
	sidplayfp engine;
	Args args;
	std::unique_ptr<ReSIDfpBuilder> rs;
	Wav<short> wav;
	std::vector<short> buffer;
	NoteState noteState;
	int minFreq;
	int maxFreq;
	float timeS;
	float ticksPerSeconds;
	int oldTimeT;
	int samplesProcessed;
	int samplesToPorcess;
	int samplesBeforeFadeout;
public:
	SidReader(Song &song);
	~SidReader();
	vector<char> loadRom(const char* path, size_t romSize);
	void beginProcess(Args &_args);
	float process();
	void finish();
};

