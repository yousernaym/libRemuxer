//Wav file format specs:
//http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html

#pragma once
#include <cmath>
#include <cstdint>
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
	//normalize = true peak-normalises to -1 dB (used for the mixdown).
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

	//Downmix to mono and write unsigned 8-bit PCM. Used for per-channel solo WAVs (compact cache);
	//unnormalized so lane levels stay comparable across shared channels.
	bool writeFile8BitMono(const std::string &path, const std::vector<SampleType> &data)
	{
		size_t frameCount = data.size() / (size_t)channelCount;
		std::vector<uint8_t> pcm(frameCount);
		for (size_t f = 0; f < frameCount; f++)
		{
			float s = channelCount == 2
				? 0.5f * (data[f * 2] + data[f * 2 + 1])
				: data[f];
			if (s < -1.f) s = -1.f;
			else if (s > 1.f) s = 1.f;
			int v = (int)std::lround(s * 127.0f);
			if (v < -128) v = -128;
			else if (v > 127) v = 127;
			pcm[f] = (uint8_t)(v + 128);
		}

		int dataSize = (int)pcm.size();
		int pad = dataSize & 1; // WAV data chunk is word-aligned; pad is not part of dataSize
		if (!createFile(path))
			return false;
		outFile << "RIFF";
		writeLE(4, 36 + dataSize + pad);
		outFile << "WAVEfmt ";
		writeLE(4, 16);             // PCM fmt chunk size
		writeLE(2, 1);              // PCM
		writeLE(2, 1);              // mono
		writeLE(4, sampleRate);
		writeLE(4, sampleRate);     // byte rate (1 byte/sample)
		writeLE(2, 1);              // block align
		writeLE(2, 8);              // bits per sample
		outFile << "data";
		writeLE(4, dataSize);
		if (dataSize > 0)
			writeChunk(pcm.data(), dataSize);
		if (pad)
			writeByte(0);
		outFile.close();
		return true;
	}

	bool saveFile(const std::string &path, bool normalize = true)
	{
		return writeFile(path, samples, normalize);
	}

	bool saveFile8BitMono(const std::string &path)
	{
		return writeFile8BitMono(path, samples);
	}
};

