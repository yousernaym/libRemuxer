#pragma once

#include "Song.h"
#include "wav.h"

class SongReader
{
protected:
	const int sampleRate = 48000;
	Song &song;
	Wav wav;
	std::vector<SampleType> sampleBuffer;
public:
	SongReader(Song& song, bool stereo) :
		song(song), wav(stereo, sampleRate), sampleBuffer(500)
	{

	}
	virtual ~SongReader() {}
	void beginProcessing()
	{
		wav.clearSamples();
		sampleBuffer.clear();
	}
	virtual float process() = 0;
	virtual void finish() = 0;
};

