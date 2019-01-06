#pragma once
#include <fstream>
#include <vector>

template <class SampleType>
class Wav
{
private:
	std::ofstream outFile;
	const int sampleRate = 44100;
	const int channelCount = 2;
	std::vector<SampleType> samples;
	template<class T>
	void writeNumber(T number)
	{
		outFile.write(reinterpret_cast<const char*>(&number), sizeof(number));
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
		writeNumber<int>(36 + dataSize);
		outFile << "WAVEfmt ";
		writeNumber<int>(16);
		writeNumber<short>(1);
		writeNumber<short>(2);
		writeNumber<int>(sampleRate);
		writeNumber<int>(sampleRate * blockAlign);
		writeNumber<short>(blockAlign);
		writeNumber<short>(sizeof(SampleType) * 8);
		outFile << "data";
		writeNumber<int>(dataSize);
		//samples.clear();
		//samples = vector<short>(1000);
		outFile.write(reinterpret_cast<const char*>(&samples[0]), dataSize);
		outFile.close();
	}
};

