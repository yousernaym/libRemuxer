#pragma once

// HAVE_CXX11/14/17 (needed by libsidplayfp's sidcxx11.h, since MSVC does not set
// __cplusplus reliably) are defined centrally in libRemuxer.vcxproj.

#include "song.h"
#include "libRemuxer.h"
#include "SongReader.h"
#include "Wav.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <sidplayfp/sidplayfp.h>
#include <builders/residfp-builder/residfp.h> 

class SidReader : public SongReader
{
	sidplayfp engine;
	std::unique_ptr<ReSIDfpBuilder> rs;
	std::array<uint8_t, 32> sidRegs;
	std::array<bool, 3> gateState;
	std::vector<short> sidAudioBuffer;
	int minFreq;
	int maxFreq;
	float timeS;
	float ticksPerSeconds;
	int oldTimeT;
	int samplesProcessed;
	int samplesToProcess;
	int samplesBeforeFadeout;
public:
	SidReader(Song &song);
	~SidReader();
	std::vector<char> loadRom(const char* path, size_t romSize);
	void beginProcess(UserArgs &_args);
	float process() override;
	void endProcessing() override;
};

