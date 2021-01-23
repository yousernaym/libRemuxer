#pragma once

#include "Song.h"
#include "wav.h"

class SongReader
{
protected:
	UserArgs userArgs;
	const int sampleRate = 48000;
	Song &song;
	std::vector<SampleType> sampleBuffer;
	Wav wav;
	
	void beginProcessing(const UserArgs& args)
	{
		userArgs = args;
		wav.clearSamples();
		std::fill(sampleBuffer.begin(), sampleBuffer.end(), 0);
	}
public:
	SongReader(Song& song, bool stereo) :
		song(song), wav(stereo, sampleRate), sampleBuffer(500)
	{
	}
	virtual ~SongReader() {}
	virtual float process() = 0;
	virtual void finish()
	{
		if (userArgs.audioPath[0] != 0)
			wav.saveFile(userArgs.audioPath);
	}
};

