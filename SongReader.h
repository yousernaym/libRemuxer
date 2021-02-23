#pragma once

#include "Song.h"
#include "wav.h"

class SongReader
{
	void clearAudioBuffer()
	{
		std::fill(audioBuffer.begin(), audioBuffer.end(), (SampleType)0);
	}
protected:
	UserArgs userArgs;
	const int sampleRate = 48000;
	Song &song;
	std::vector<SampleType> audioBuffer;
	Wav wav;
		
	void beginProcessing(const UserArgs& args)
	{
		clearAudioBuffer();
		userArgs = args;
		wav.clearSamples();
	}
	
	void setAudioBufferSize(size_t size)
	{
		audioBuffer.resize(size);
		clearAudioBuffer();
	}
public:
	SongReader(Song& song, bool stereo, int audioBufferSize = 500) :
		song(song), wav(stereo, sampleRate)
	{
		setAudioBufferSize(audioBufferSize * (stereo ? 2 : 1));
	}
	virtual ~SongReader() {}
	virtual float process() = 0;
	virtual void endProcessing()
	{
		if (userArgs.audioPath[0] != 0)
			wav.saveFile(userArgs.audioPath);
	}
};

