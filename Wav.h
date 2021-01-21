#pragma once
#include <fstream>
#include <vector>
#include "FileFormat.h"

template <class SampleType>
class Wav : FileFormat
{
private:
	int sampleRate;
	const int channelCount = 1;
	std::vector<SampleType> samples;
public:
	Wav(int sampleRate = 48000) : sampleRate(sampleRate)
	{
		samples.reserve(channelCount * 500 * sampleRate); //Reserve space for 500 seconds
	}
	~Wav()
	{
	}
	
	void addSamples(const std::vector<SampleType> &newSamples)
	{
		samples.insert(samples.end(), newSamples.begin(), newSamples.end());
	}

	void clearSamples()
	{
		samples.clear();
	}

	void saveFile(const std::string &path)
	{
		//Normalize
		short peak = 0;
		for (short sample : samples)
		{
			if (peak < abs(sample))
				peak = abs(sample);
		}
		float normalizingScale = SHRT_MAX * 0.89125f / peak; //Leave 1 dB headroom for any lossy compression that might occur
		for (short& sample : samples)
			sample = (short)(sample * normalizingScale);

		int blockAlign = sizeof(SampleType) * channelCount;
		int dataSize = (int)samples.size() * sizeof(SampleType);
		createFile(path);
		outFile << "RIFF";
		writeLE(4, 36 + dataSize);
		outFile << "WAVEfmt ";
		writeLE(4, 16);
		writeLE(2, 1);
		writeLE(2, channelCount);
		writeLE(4, sampleRate);
		writeLE(4, sampleRate * blockAlign);
		writeLE(2, blockAlign);
		writeLE(2, sizeof(SampleType) * 8);
		outFile << "data";
		writeLE(4, dataSize);
		writeChunk(&samples[0], dataSize);
		outFile.close();
	}
};

