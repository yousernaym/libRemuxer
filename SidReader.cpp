#include "SidReader.h"

#include <fcntl.h>
#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
#include <iostream>
#include <array>
#include <map>

#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidConfig.h>
#include <sidemu.h>
#include <sidplayfp/SidtuneInfo.h>

#define KERNAL_PATH  "roms\\kernal.901227-03.bin"
#define BASIC_PATH   "roms\\basic.901226-01.bin"
#define CHARGEN_PATH "roms\\characters.901225-01.bin"

std::map<int, std::string> waveformNames = { {1, "Triangle"}, {2, "Sawtooth"}, {4, "Pulse"}, {8, "Noise"} };
std::set<int> usedWaveformCombos;
const bool USE_STEREO = true;
const int AUDIO_CHANNEL_COUNT = USE_STEREO ? 2 : 1;

SidReader::SidReader(Song &song) : SongReader(song, USE_STEREO)
{
	// Load ROM files
	auto kernal = loadRom(KERNAL_PATH, 8192);
	auto basic = loadRom(BASIC_PATH, 8192);
	auto chargen = loadRom(CHARGEN_PATH, 4096);

	engine.setRoms((const uint8_t*)&kernal[0], (const uint8_t*)&basic[0], (const uint8_t*)&chargen[0]);

	// Set up a SID builder
	rs = std::make_unique<ReSIDfpBuilder>("ResidflBuilder");

	// Get the number of SIDs supported by the engine
	unsigned int maxsids = (engine.info()).maxsids();

	// Create SID emulators
	rs->create(maxsids);

	// Check if builder is ok
	if (!rs->getStatus())
		throw rs->error();
}

SidReader::~SidReader()
{
}

/*
 * Load ROM dump from file.
 * Allocate the buffer if file exists, otherwise return 0.
 */
std::vector<char> SidReader::loadRom(const char* path, size_t romSize)
{
	std::vector<char> buffer;
	std::ifstream is(path, std::ios::binary);
	if (is.good())
	{
		buffer.resize(romSize);
		is.read(&buffer[0], romSize);
	}
	is.close();
	return buffer;
}
void SidReader::beginProcess(UserArgs &args)
{
	SongReader::beginProcessing(args);
	//args.songLengthS = 3;
	if (userArgs.songLengthS == 0)
		userArgs.songLengthS = 300;
	float fadeOutS = 7;
	userArgs.songLengthS += fadeOutS;

	song.marSong->ticksPerBeat = 240; 
	song.marSong->tempoEvents[0].tempo = 125;
	song.marSong->numTempoEvents = 1;
	ticksPerSeconds = (float)(song.marSong->tempoEvents[0].tempo * song.marSong->ticksPerBeat / 60.);
	song.marSong->tempoEvents[0].time = 0;

	song.tracks.resize(3);
	for (int i = 0; i < 3; i++)
	{
		song.tracks[i].ticks.resize(int((userArgs.songLengthS + (float)audioBuffer.size() / sampleRate) * ticksPerSeconds) + 1);
		if (!userArgs.insTrack)
			sprintf_s(song.marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, "Channel %i", i + 1);
	}
		
	// Load tune from file
	std::unique_ptr<SidTune> tune(new SidTune(userArgs.inputPath));
	
	// CHeck if the tune is valid
	if (!tune->getStatus())
		throw tune->statusString();

	auto tuneInfo = tune->getInfo();
	if (tuneInfo->clockSpeed() == SidTuneInfo::CLOCK_NTSC)
	{
		minFreq = 268;
		maxFreq = 64832;
	}
	else
	{
		minFreq = 279;
		maxFreq = 65535;
	}
	args.numSubSongs = tuneInfo->songs();
	
	// Select default song
	args.subSong = tune->selectSong(userArgs.subSong);
	
	// Configure the engine
	SidConfig cfg;
	cfg.frequency = sampleRate;
	//cfg.samplingMethod = SidConfig::INTERPOLATE;
	cfg.samplingMethod = SidConfig::RESAMPLE_INTERPOLATE;
	cfg.fastSampling = false;
	cfg.playback = USE_STEREO ? SidConfig::STEREO : SidConfig::MONO;
	cfg.sidEmulation = rs.get();
	cfg.disableAudio = userArgs.audioPath[0] == 0;
	//cfg.forceSidModel = true;
	//cfg.forceC64Model = true;
	//cfg.defaultC64Model = SidConfig::c64_model_t::DREAN;
	//cfg.defaultSidModel = SidConfig::sid_model_t::MOS8580;

	if (!engine.config(cfg))
		throw engine.error();

	// Load tune into engine
	if (!engine.load(tune.get()))
		throw engine.error();

	timeS = 0;
	oldTimeT = 0;
	samplesProcessed = 0;
	samplesToProcess = (int)(sampleRate * userArgs.songLengthS) * AUDIO_CHANNEL_COUNT;
	samplesBeforeFadeout = samplesToProcess - (int)(fadeOutS * sampleRate);
}

float SidReader::process()
{
	samplesProcessed += engine.play(audioBuffer.data(), (uint_least32_t)audioBuffer.size());
	timeS = (float)samplesProcessed / sampleRate;
	timeS /= AUDIO_CHANNEL_COUNT;
	int timeT = (int)(timeS * ticksPerSeconds);
	if (timeT > oldTimeT)
	{
		for (int c = 0; c < 3; c++)
		{
			for (int t = oldTimeT + 1; t < timeT; t++)
				song.tracks[c].ticks[t] = song.tracks[c].ticks[oldTimeT];

			RunningTickInfo &curTick = song.tracks[c].ticks[timeT];
			RunningTickInfo &prevTick = *song.tracks[c].getPrevTick(timeT);

			curTick.noteStart = prevTick.noteStart;
			engine.getNoteState(noteState, c);

			int freq = noteState.frequency;

			//noteState.isPlaying = true;
			//noteState.gateChanged = false;
			if (noteState.waveform > 0 && noteState.volume > 0 && freq >= minFreq && freq <= maxFreq)
			{
				curTick.notePitch = (int)(log2((float)freq / minFreq) * 12 + 0.5f) + 1;
				if (prevTick.vol == 0 || prevTick.notePitch != curTick.notePitch || noteState.gateChanged && noteState.gate)
					curTick.noteStart = timeT;
				curTick.ins = noteState.waveform;
				usedWaveformCombos.insert(curTick.ins);
				curTick.vol = noteState.volume;
				//if (noteState.gateChanged && noteState.gate)
					//prevTick.vol = 0;
			}
			else
				curTick.vol = 0;
		}
	}

	oldTimeT = timeT;
	if (userArgs.audioPath[0] != 0)
	{
		//Fade out
		if (samplesProcessed > samplesBeforeFadeout)
		{
			float scale = (float)(samplesToProcess - samplesProcessed) / (samplesToProcess - samplesBeforeFadeout);
			for (int i = 0; i < audioBuffer.size(); i++)
				audioBuffer[i] *= scale;
		}

		wav.addSamples(audioBuffer);
	}

	if (samplesProcessed < samplesToProcess)
		return (float)samplesProcessed / samplesToProcess;
	else
	{
		return -1;
	}
}

void SidReader::endProcessing()
{
	SongReader::endProcessing();
	
	if (userArgs.insTrack)
	{
		//Use waveform names as track names
		int t = 1;
		for each(int waveformCombo in usedWaveformCombos)
		{
			//First waveform
			std::string trackName = waveformNames[waveformCombo & 1];
			
			//Loop through the remaining 3 waveform bits
			for (int i = 1; i < 4; i++)
			{
				int maskedWaveform = waveformCombo & (1 << i);
				if (maskedWaveform)
					trackName += (trackName.empty() ? "" : " + ") + waveformNames[maskedWaveform];
			}
			strcpy_s(song.marSong->tracks[t].name, MAX_TRACKNAME_LENGTH, trackName.c_str());
			t++;
		}
	}
	song.createNoteList(userArgs, &usedWaveformCombos);
}



