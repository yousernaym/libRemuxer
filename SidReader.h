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

class SidTune;

class SidReader : public SongReader
{
	// Per-track pass descriptor. voice = chip-0 voice for per-channel passes;
	// combo = target waveform combo for per-instrument passes.
	struct TrackPass
	{
		int midiTrack;
		int voice;
		int combo;
	};

	sidplayfp engine;
	std::unique_ptr<ReSIDfpBuilder> rs;
	std::unique_ptr<SidTune> tune;
	int selectedSong = 0;
	std::array<uint8_t, 32> sidRegs;
	std::array<uint8_t, 3> sidEnvelopes;
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

	bool notesFinalized = false;
	int curPass = 0; // 0 = main pass (note extraction + mixdown); >=1 = track passes
	std::vector<TrackPass> trackPasses;

	void finalizeNotes();
	void buildTrackPasses();
	void startTrackPass(int passIndex);
	bool renderMainChunk();   // extracts ticks + renders mixdown; true when the main pass is done
	bool renderTrackChunk();  // renders one muted track pass chunk; true when the pass is done
public:
	SidReader(Song &song);
	~SidReader();
	std::vector<char> loadRom(const char* path, size_t romSize);
	void beginProcess(UserArgs &_args);
	float process() override;
	void endProcessing() override;
};

