#include "SidReader.h"

#include <fcntl.h>
#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
#include <iostream>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
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

namespace
{
	constexpr int SidVoiceRegisterStride = 7;
	constexpr int SidControlRegisterOffset = 4;
	constexpr int SidWaveformShift = 4;
	constexpr int MaxSidPlayCycles = 20000;
	constexpr float ShortSampleScale = 1.0f / 32768.0f;

	template<typename Config>
	void setPlayback(Config& cfg)
	{
		if constexpr (requires(Config& c) { Config::STEREO; Config::MONO; c.playback; })
			cfg.playback = USE_STEREO ? Config::STEREO : Config::MONO;
	}

	template<typename Config>
	void setFastSampling(Config& cfg, bool enabled)
	{
		if constexpr (requires(Config& c) { c.fastSampling; })
			cfg.fastSampling = enabled;
	}

	template<typename Engine>
	void initMixer(Engine& engine)
	{
		if constexpr (requires(Engine& e) { e.initMixer(true); })
			engine.initMixer(USE_STEREO);
	}

	template<typename Engine>
	uint_least32_t playSid(Engine& engine, std::vector<SampleType>& audioBuffer, std::vector<short>& sidAudioBuffer)
	{
		if constexpr (requires(Engine& e, short* buffer, uint_least32_t count) { e.play(buffer, count); })
		{
			if (sidAudioBuffer.size() != audioBuffer.size())
				sidAudioBuffer.resize(audioBuffer.size());

			uint_least32_t sampleCount = engine.play(sidAudioBuffer.data(), (uint_least32_t)sidAudioBuffer.size());
			for (uint_least32_t i = 0; i < sampleCount; i++)
				audioBuffer[i] = sidAudioBuffer[i] * ShortSampleScale;
			return sampleCount;
		}
		else if constexpr (requires(Engine& e, SampleType* buffer, uint_least32_t count) { e.play(buffer, count); })
		{
			return engine.play(audioBuffer.data(), (uint_least32_t)audioBuffer.size());
		}
		else if constexpr (requires(Engine& e, unsigned int cycles) { e.play(cycles); } &&
			requires(Engine& e, short* buffer, unsigned int samples) { e.mix(buffer, samples); })
		{
			int sampleFrames = engine.play(MaxSidPlayCycles);
			if (sampleFrames < 0)
				throw engine.error();

			if (sampleFrames == 0)
				return 0;

			size_t outputSamples = (size_t)sampleFrames * AUDIO_CHANNEL_COUNT;
			if constexpr (requires(Engine& e, unsigned int cycles) { e.getBufSize(cycles); })
				outputSamples = (std::max)(outputSamples, (size_t)engine.getBufSize(MaxSidPlayCycles));

			if (sidAudioBuffer.size() < outputSamples)
				sidAudioBuffer.resize(outputSamples);
			if (audioBuffer.size() < outputSamples)
				audioBuffer.resize(outputSamples);

			unsigned int mixedSamples = engine.mix(sidAudioBuffer.data(), (unsigned int)sampleFrames);
			for (unsigned int i = 0; i < mixedSamples; i++)
				audioBuffer[i] = sidAudioBuffer[i] * ShortSampleScale;
			return mixedSamples;
		}
	}

	struct SidVoiceRegisters
	{
		int frequency;
		bool gate;
		bool gateChanged;
		int volume;
		int waveform;
	};

	SidVoiceRegisters readVoiceRegisters(const std::array<uint8_t, 32>& regs, int channel, bool previousGate)
	{
		int offset = channel * SidVoiceRegisterStride;
		uint8_t control = regs[offset + SidControlRegisterOffset];
		bool gate = (control & 0x01) != 0;

		SidVoiceRegisters voice;
		voice.frequency = regs[offset] | (regs[offset + 1] << 8);
		voice.gate = gate;
		voice.gateChanged = gate != previousGate;
		voice.volume = gate ? 255 : 0;
		voice.waveform = (control >> SidWaveformShift) & 0x0f;
		return voice;
	}
}

SidReader::SidReader(Song &song) : SongReader(song, USE_STEREO)
{
	// Load ROM files
	auto kernal = loadRom(KERNAL_PATH, 8192);
	auto basic = loadRom(BASIC_PATH, 8192);
	auto chargen = loadRom(CHARGEN_PATH, 4096);

	engine.setRoms((const uint8_t*)&kernal[0], (const uint8_t*)&basic[0], (const uint8_t*)&chargen[0]);

	// Set up a SID builder. In libsidplayfp 3.x the engine locks the SID
	// emulators it needs internally during config(), so the builder no longer
	// exposes create(count)/getStatus(); any failure surfaces via engine.config().
	rs = std::make_unique<ReSIDfpBuilder>("ResidflBuilder");
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

	song.songData->ticksPerBeat = 240; 
	song.songData->tempoEvents[0].tempo = 125;
	song.songData->numTempoEvents = 1;
	ticksPerSeconds = (float)(song.songData->tempoEvents[0].tempo * song.songData->ticksPerBeat / 60.);
	song.songData->tempoEvents[0].time = 0;

	song.tracks.resize(3);
	for (int i = 0; i < 3; i++)
	{
		song.tracks[i].ticks.resize(int((userArgs.songLengthS + (float)audioBuffer.size() / sampleRate) * ticksPerSeconds) + 1);
		if (!userArgs.insTrack)
			sprintf_s(song.songData->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, "Channel %i", i + 1);
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
	setFastSampling(cfg, false);
	setPlayback(cfg);
	cfg.sidEmulation = rs.get();
	//cfg.forceSidModel = true;
	//cfg.forceC64Model = true;
	//cfg.defaultC64Model = SidConfig::c64_model_t::DREAN;
	//cfg.defaultSidModel = SidConfig::sid_model_t::MOS8580;

	if (!engine.config(cfg))
		throw engine.error();

	// Load tune into engine
	if (!engine.load(tune.get()))
		throw engine.error();
	initMixer(engine);

	timeS = 0;
	oldTimeT = 0;
	samplesProcessed = 0;
	samplesToProcess = (int)(sampleRate * userArgs.songLengthS) * AUDIO_CHANNEL_COUNT;
	samplesBeforeFadeout = samplesToProcess - (int)(fadeOutS * sampleRate);
	sidRegs.fill(0);
	gateState.fill(false);
	sidAudioBuffer.resize(audioBuffer.size());
}

float SidReader::process()
{
	uint_least32_t generatedSamples = playSid(engine, audioBuffer, sidAudioBuffer);
	if (generatedSamples == 0)
		return -1;

	samplesProcessed += generatedSamples;
	timeS = (float)samplesProcessed / sampleRate;
	timeS /= AUDIO_CHANNEL_COUNT;
	int timeT = (int)(timeS * ticksPerSeconds);
	if (timeT > oldTimeT)
	{
		bool gotSidRegisters = engine.getSidStatus(0, sidRegs.data());
		for (int c = 0; c < 3; c++)
		{
			for (int t = oldTimeT + 1; t < timeT; t++)
				song.tracks[c].ticks[t] = song.tracks[c].ticks[oldTimeT];

			RunningTickInfo &curTick = song.tracks[c].ticks[timeT];
			RunningTickInfo &prevTick = *song.tracks[c].getPrevTick(timeT);

			curTick.noteStart = prevTick.noteStart;
			if (!gotSidRegisters)
			{
				curTick.vol = 0;
				continue;
			}

			SidVoiceRegisters voice = readVoiceRegisters(sidRegs, c, gateState[c]);
			gateState[c] = voice.gate;

			if (voice.waveform > 0 && voice.volume > 0 && voice.frequency >= minFreq && voice.frequency <= maxFreq)
			{
				curTick.notePitch = (int)(log2((float)voice.frequency / minFreq) * 12 + 0.5f) + 1;
				if (prevTick.vol == 0 || prevTick.notePitch != curTick.notePitch || (voice.gateChanged && voice.gate))
					curTick.noteStart = timeT;
				curTick.ins = voice.waveform;
				usedWaveformCombos.insert(curTick.ins);
				curTick.vol = voice.volume;
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
			for (uint_least32_t i = 0; i < generatedSamples; i++)
				audioBuffer[i] *= scale;
		}

		for (uint_least32_t i = 0; i < generatedSamples; i++)
			wav.addSample(audioBuffer[i]);
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
		for (int waveformCombo : usedWaveformCombos)
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
			strcpy_s(song.songData->tracks[t].name, MAX_TRACKNAME_LENGTH, trackName.c_str());
			t++;
		}
	}
	song.createNoteList(userArgs, &usedWaveformCombos);
}



