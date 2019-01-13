#pragma once
#include "song.h"
#include "remuxer.h"
#include "SongReader.h"
#include "Wav.h"
#include <sidplayfp/sidplayfp.h>
#include <builders/residfp-builder/residfp.h> 

class SidReader : public SongReader
{
	sidplayfp m_engine;
	std::unique_ptr<ReSIDfpBuilder> rs;
	Wav<short> wav;
	const int bufferSize = 500;
	std::vector<short> buffer;
	NoteState noteState;
	float freqConSt;
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
	void beginProcess(const Args &args);
	float process();
};

