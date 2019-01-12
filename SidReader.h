#pragma once
#include "song.h"
#include "remuxer.h"
#include "SongReader.h"
#include "Wav.h"
#include <sidplayfp/sidplayfp.h>

class SidReader : public SongReader
{
	sidplayfp m_engine;
	
	Wav<short> wav;
	const int bufferSize = 500;
	std::vector<short> buffer;
	NoteState noteState;
	float freqConSt;
	float minFreq;
	float maxFreq;
	float timeS;
	float ticksPerSeconds;
	int oldTimeT;
	int samplesProcessed;
	int samplesToPorcess;
	int samplesBeforeFadeout;
public:
	SidReader(Song &song);
	~SidReader();
	char* loadRom(const char* path, size_t romSize);
	void beginProcess(const Args &args);
	float process();
};

