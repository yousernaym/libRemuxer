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
#include <vm-extensions/libsidplayfp-volenv.h>

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
	//Poll interval for note extraction. Must stay well under one PAL frame (19656 cycles):
	//at ~20000 cycles the sampling aliased against the 50 Hz driver writes, skipping frames and
	//merging fast arpeggio steps. ~1 ms also keeps gate off->on retriggers between polls rare.
	constexpr int MaxSidPlayCycles = 1000;
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

	SidVoiceRegisters readVoiceRegisters(const std::array<uint8_t, 32>& regs,
		const std::array<uint8_t, 3>& envelopes,
		bool hasEnvelopes,
		int channel,
		bool previousGate)
	{
		int offset = channel * SidVoiceRegisterStride;
		uint8_t control = regs[offset + SidControlRegisterOffset];
		bool gate = (control & 0x01) != 0;

		SidVoiceRegisters voice;
		voice.frequency = regs[offset] | (regs[offset + 1] << 8);
		voice.gate = gate;
		voice.gateChanged = gate != previousGate;
		voice.volume = hasEnvelopes ? envelopes[channel] : (gate ? 255 : 0);
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
	usedWaveformCombos.clear(); //file-scope global; must be fresh for each conversion
	notesFinalized = false;
	curPass = 0;
	trackPasses.clear();
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
		
	// Load tune from file. Kept as a member so it outlives beginProcess: engine.load keeps a
	// reference to the tune, and each track pass re-loads it.
	tune = std::make_unique<SidTune>(userArgs.inputPath);

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
	selectedSong = args.subSong;

	// Configure the engine
	SidConfig cfg;
	cfg.frequency = sampleRate;
	//cfg.samplingMethod = SidConfig::INTERPOLATE;
	cfg.samplingMethod = SidConfig::RESAMPLE_INTERPOLATE;
	setFastSampling(cfg, false);
	setPlayback(cfg);
	cfg.powerOnDelay = 0; //deterministic per-pass start so track passes align sample-for-sample with the mixdown
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
	sidEnvelopes.fill(0);
	gateState.fill(false);
	attackState.fill(false);
	pendingRetrigger.fill(false);
	sidAudioBuffer.resize(audioBuffer.size());
}

//Main pass: render one chunk, extract note ticks, and (if requested) accumulate the mixdown.
//Returns true when the configured song length has been rendered.
bool SidReader::renderMainChunk()
{
	uint_least32_t generatedSamples = playSid(engine, audioBuffer, sidAudioBuffer);
	if (generatedSamples == 0)
		return true;

	samplesProcessed += generatedSamples;

	//Latch envelope ATTACK rising edges every chunk (~1 ms). reSIDfp only enters ATTACK on a
	//gate 0->1 write, so this catches retriggers whose gate off+on both happen between two
	//register polls (invisible to the gateState comparison below). Consumed per recorded tick.
	std::array<bool, 3> attackNow{};
	if (libsidplayfp::vm_getSidEnvelopeAttackStates(engine, 0, attackNow.data()))
	{
		for (int c = 0; c < 3; c++)
		{
			if (attackNow[c] && !attackState[c])
				pendingRetrigger[c] = true;
			attackState[c] = attackNow[c];
		}
	}

	timeS = (float)samplesProcessed / sampleRate;
	timeS /= AUDIO_CHANNEL_COUNT;
	int timeT = (int)(timeS * ticksPerSeconds);
	if (timeT > oldTimeT)
	{
		bool gotSidRegisters = engine.getSidStatus(0, sidRegs.data());
		bool gotSidEnvelopes = libsidplayfp::vm_getSidEnvelopes(engine, 0, sidEnvelopes.data());
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

			SidVoiceRegisters voice = readVoiceRegisters(sidRegs, sidEnvelopes, gotSidEnvelopes, c, gateState[c]);
			gateState[c] = voice.gate;
			bool retriggered = pendingRetrigger[c];
			pendingRetrigger[c] = false;

			bool validVoice = voice.waveform > 0 && voice.frequency >= minFreq && voice.frequency <= maxFreq;

			//Record any audible tick regardless of gate, always reading pitch/waveform from the
			//current registers (matches v0.4.0). Fast-note drivers (e.g. Giana Sisters) gate off
			//after one frame and play the rest of a run during the release phase, changing the
			//frequency register every frame; freezing pitch during release would collapse such a
			//run into one long note at the first pitch and drop the rest.
			if (validVoice && voice.volume > 0)
			{
				curTick.notePitch = (int)(log2((float)voice.frequency / minFreq) * 12 + 0.5f) + 1;
				if (prevTick.vol == 0 || prevTick.notePitch != curTick.notePitch || (voice.gateChanged && voice.gate) || retriggered)
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

	return samplesProcessed >= samplesToProcess;
}

//Track pass: render one chunk with the pass's voices muted (dynamically for per-instrument
//passes), accumulate the per-track WAV. Never writes note ticks or usedWaveformCombos.
bool SidReader::renderTrackChunk()
{
	const TrackPass &tp = trackPasses[curPass - 1];

	//Per-instrument passes: re-derive the mute mask each chunk from the current waveform of each
	//voice (same register reads the note extractor uses). true = muted.
	if (userArgs.insTrack)
	{
		if (engine.getSidStatus(0, sidRegs.data()))
		{
			for (int v = 0; v < 3; v++)
			{
				uint8_t control = sidRegs[v * SidVoiceRegisterStride + SidControlRegisterOffset];
				int waveform = (control >> SidWaveformShift) & 0x0f;
				engine.mute(0, v, waveform != tp.combo);
			}
		}
	}

	uint_least32_t generatedSamples = playSid(engine, audioBuffer, sidAudioBuffer);
	if (generatedSamples == 0)
		return true;

	samplesProcessed += generatedSamples;

	//Fade out (same envelope as the mixdown so per-track WAVs match its length).
	if (samplesProcessed > samplesBeforeFadeout)
	{
		float scale = (float)(samplesToProcess - samplesProcessed) / (samplesToProcess - samplesBeforeFadeout);
		for (uint_least32_t i = 0; i < generatedSamples; i++)
			audioBuffer[i] *= scale;
	}
	for (uint_least32_t i = 0; i < generatedSamples; i++)
		wav.addSample(audioBuffer[i]);

	return samplesProcessed >= samplesToProcess;
}

//Name instrument tracks (per-instrument mode) and build the note/MIDI output. Idempotent.
void SidReader::finalizeNotes()
{
	if (notesFinalized)
		return;
	notesFinalized = true;

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

//Build the per-track pass list after finalizeNotes (track numbers/note counts are then known).
void SidReader::buildTrackPasses()
{
	trackPasses.clear();
	if (!trackAudioRequested())
		return;

	if (userArgs.insTrack)
	{
		//One pass per used waveform combo, in usedWaveformCombos order (matches createNoteList's
		//track compaction); midiTrack = 1-based index. Skip empty tracks.
		int t = 1;
		for (int combo : usedWaveformCombos)
		{
			if (song.songData->tracks[t].numNotes > 0)
				trackPasses.push_back({ t, -1, combo });
			t++;
		}
	}
	else
	{
		//One pass per chip-0 voice (0..2) that produced notes; midiTrack = voice + 1.
		for (int v = 0; v < 3; v++)
		{
			int midiTrack = v + 1;
			if (song.songData->tracks[midiTrack].numNotes > 0)
				trackPasses.push_back({ midiTrack, v, -1 });
		}
	}
}

//Reset the engine for a track pass: full C64 reset via reload, then set the initial mute mask.
void SidReader::startTrackPass(int passIndex)
{
	const TrackPass &tp = trackPasses[passIndex];

	tune->selectSong(selectedSong);
	engine.load(tune.get()); //forces full C64 reset + reconfig
	initMixer(engine);       //chip buffers recreated

	samplesProcessed = 0;
	oldTimeT = 0;
	timeS = 0;
	gateState.fill(false);
	attackState.fill(false);
	pendingRetrigger.fill(false);

	//isMuted is only reset in the sidemu constructor and emus may be reused, so set every slot
	//explicitly every pass. Mute all 4 voice slots (0..2 voices, 3 = digi/samples) on all 3 chips.
	for (unsigned int sid = 0; sid < 3; sid++)
		for (unsigned int v = 0; v < 4; v++)
			engine.mute(sid, v, true);

	if (!userArgs.insTrack)
		engine.mute(0, (unsigned int)tp.voice, false); //per-channel: only the target voice audible
	//Per-instrument passes start fully muted; renderTrackChunk unmutes voices dynamically.
	//Voice 3 (digi) and chips 1-2 stay muted in all track passes.
}

float SidReader::process()
{
	if (curPass == 0)
	{
		bool done = renderMainChunk();
		if (done)
		{
			finalizeNotes();
			buildTrackPasses();
			saveMixdownNow();
			curPass = 1;
			if (trackPasses.empty())
				return -1;
			startTrackPass(0);
			return clampProgress(1.f / (1 + (int)trackPasses.size()));
		}
		//Estimate total passes while the combo/voice set is still filling in.
		int estPasses = userArgs.insTrack
			? 1 + (std::max)((int)usedWaveformCombos.size(), 1)
			: 1 + 3;
		float mainFrac = samplesToProcess > 0 ? (float)samplesProcessed / samplesToProcess : 1;
		return clampProgress(mainFrac / estPasses);
	}
	else
	{
		bool done = renderTrackChunk();
		int totalPasses = 1 + (int)trackPasses.size();
		if (done)
		{
			const TrackPass &tp = trackPasses[curPass - 1];
			saveTrackWav(tp.midiTrack, song.songData->tracks[tp.midiTrack].name);
			curPass++;
			if (curPass - 1 >= (int)trackPasses.size())
				return -1;
			startTrackPass(curPass - 1);
			return clampProgress((float)curPass / totalPasses);
		}
		float frac = samplesToProcess > 0 ? (float)samplesProcessed / samplesToProcess : 1;
		return clampProgress((curPass + frac) / totalPasses);
	}
}

void SidReader::endProcessing()
{
	//Cancel-during-main-pass path: notes were never finalized, so do it now (partial import).
	finalizeNotes();
	//Saves the (partial) mixdown only if a track pass hasn't already saved the full one.
	SongReader::endProcessing();
}



