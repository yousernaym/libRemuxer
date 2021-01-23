//Wav file format specs:
//http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html

#pragma once
#include <fstream>
#include <vector>
#include "FileFormat.h"

typedef float SampleType;

class Wav : FileFormat
{
private:
	const int sampleRate;
	const int channelCount;
	std::vector<SampleType> samples;
public:
	Wav(bool stereo, int sampleRate = 48000) :
		channelCount(stereo ? 2 : 1), sampleRate(sampleRate)
	{
		samples.reserve(channelCount * 500 * sampleRate); //Reserve space for 500 seconds
	}
	~Wav()
	{
	}
	
	void addSample(SampleType newSample)
	{
		samples.push_back(newSample);
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
		bool isFloat = std::is_floating_point<SampleType>::value;

		//Normalize
		SampleType peak = 0;
		for (SampleType sample : samples)
		{
			if (peak < abs(sample))
				peak = abs(sample);
		}
		float max = 1;
		if (!isFloat)
			max = 1 << ((sizeof(SampleType) * 8) - 1);
		float normalizingScale = max * 0.89125f / peak; //Leave 1 dB headroom for any lossy compression that might occur
		for (SampleType& sample : samples)
			sample = (SampleType)(sample * normalizingScale);

		int blockAlign = sizeof(SampleType) * channelCount;
		int dataSize = (int)samples.size() * sizeof(SampleType);
		createFile(path);
		outFile << "RIFF";	//Chunk id
		writeLE(4, 36 + dataSize);	//Chunk size
		outFile << "WAVEfmt ";  //Format + subchunk id
		writeLE(4, isFloat ? 18 : 16);  //Subchunk size
		writeLE(2, isFloat ? 3 : 1);  //Format tag, PCM or IEEE float
		writeLE(2, channelCount);
		writeLE(4, sampleRate);
		writeLE(4, sampleRate * blockAlign); //Bytes per second
		writeLE(2, blockAlign); //Bytes per sample * channelCount
		writeLE(2, sizeof(SampleType) * 8); //Bits per sample
		if (isFloat)
		{
			writeLE(2, 0);  //Size of extension
			outFile << "fact"; //Subchunk id
			writeLE(4, 4); //Subchunk size
			writeLE(4, unsigned(samples.size() * channelCount));  //Total number of samples in all channels
		}
		outFile << "data";	//Subchunk id
		writeLE(4, dataSize);
		if (dataSize % 2 == 1)	//Padding to make sample count divisible by two
		{
			samples.push_back(0);
			dataSize++;
		}
		writeChunk(&samples[0], dataSize);
		outFile.close();
	}
};

