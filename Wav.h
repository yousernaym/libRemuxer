#pragma once
#include <fstream>
#include <vector>

template <class SampleType>
class Wav
{
private:
	std::ofstream outFile;
	const int sampleRate = 48000;
	const int channelCount = 1;
	std::vector<SampleType> samples;
	void writeNumber(int bytes, int number)
	{
		outFile.write(reinterpret_cast<const char*>(&number), bytes);
	}
public:
	Wav()
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
	void createFile(const std::string &path)
	{
		int blockAlign = sizeof(SampleType) * channelCount;
		int dataSize = samples.size() * sizeof(SampleType);
		outFile.open(path, std::ios::binary | std::ios::out);
		outFile << "RIFF";
		writeNumber(4, 36 + dataSize);
		outFile << "WAVEfmt ";
		writeNumber(4, 16);
		writeNumber(2, 1);
		writeNumber(2, channelCount);
		writeNumber(4, sampleRate);
		writeNumber(4, sampleRate * blockAlign);
		writeNumber(2, blockAlign);
		writeNumber(2, sizeof(SampleType) * 8);
		outFile << "data";
		writeNumber(4, dataSize);
		//samples.clear();
		//samples = vector<short>(1000);
		outFile.write(reinterpret_cast<const char*>(&samples[0]), dataSize);
		outFile.close();
	}
};

