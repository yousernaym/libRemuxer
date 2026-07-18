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

	//Read-only view of the accumulated buffer (used by the per-channel pass save).
	const std::vector<SampleType>& getSamples() const
	{
		return samples;
	}

	//Write an arbitrary interleaved buffer as a WAV with this instance's channel count / sample rate.
	//normalize = true peak-normalises to -1 dB (used for mixdown / whole-track WAVs); voice WAVs pass
	//false so per-lane levels stay comparable and sum back to the channel signal.
	bool writeFile(const std::string &path, std::vector<SampleType> &data, bool normalize)
	{
		bool isFloat = std::is_floating_point<SampleType>::value;

		//Normalize
		if (normalize)
		{
			SampleType peak = 0;
			for (SampleType sample : data)
			{
				if (peak < abs(sample))
					peak = abs(sample);
			}
			float max = 1;
			if (!isFloat)
				max = 1 << ((sizeof(SampleType) * 8) - 1);
			if (peak > 0)
			{
				float normalizingScale = max * 0.89125f / peak; //Leave 1 dB headroom for any lossy compression that might occur
				for (SampleType& sample : data)
					sample = (SampleType)(sample * normalizingScale);
			}
		}

		int blockAlign = sizeof(SampleType) * channelCount;
		int dataSize = (int)data.size() * sizeof(SampleType);
		if (!createFile(path))
			return false;
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
			writeLE(4, unsigned(data.size() * channelCount));  //Total number of samples in all channels
		}
		outFile << "data";	//Subchunk id
		writeLE(4, dataSize);
		if (dataSize % 2 == 1)	//Padding to make sample count divisible by two
		{
			data.push_back(0);
			dataSize++;
		}
		if (dataSize > 0)
			writeChunk(data.data(), dataSize);
		outFile.close();
		return true;
	}

	bool saveFile(const std::string &path, bool normalize = true)
	{
		return writeFile(path, samples, normalize);
	}
};

