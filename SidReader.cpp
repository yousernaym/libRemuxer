#include "Wav.h"
#include "SidReader.h"

#include <fcntl.h>

#include <stdlib.h>
#include <cstring>

#include <fstream>
#include <memory>
#include <vector>
#include <iostream>

#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidConfig.h>
#include <sidplayfp/sidemu.h>
#include <sidplayfp/SidtuneInfo.h>

//#include "sidspectro\\sidfile.h"
//#include "sidspectro\\c64.h"
#define KERNAL_PATH  "roms\\kernal.901227-03.bin"
#define BASIC_PATH   "roms\\basic.901226-01.bin"
#define CHARGEN_PATH "roms\\characters.901225-01.bin"

#define SAMPLERATE 44100

SidReader::SidReader(Song &_song) : SongReader(_song), buffer(bufferSize)
{
	// Load ROM files
	auto kernal = loadRom(KERNAL_PATH, 8192);
	auto basic = loadRom(BASIC_PATH, 8192);
	auto chargen = loadRom(CHARGEN_PATH, 4096);

	m_engine.setRoms((const uint8_t*)&kernal[0], (const uint8_t*)&basic[0], (const uint8_t*)&chargen[0]);

	// Set up a SID builder
	rs = std::make_unique<ReSIDfpBuilder>("ResidflBuilder");

	// Get the number of SIDs supported by the engine
	unsigned int maxsids = (m_engine.info()).maxsids();

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
vector<char> SidReader::loadRom(const char* path, size_t romSize)
{
	vector<char> buffer;
	std::ifstream is(path, std::ios::binary);
	if (is.good())
	{
		buffer.resize(romSize);
		is.read(&buffer[0], romSize);
	}
	is.close();
	return buffer;
}
void SidReader::beginProcess(const Args &args)
{
	//g_args.songLengthS = 3;
	if (g_args.songLengthS == 0)
		g_args.songLengthS = 300;
	float fadeOutS = 7;
	g_args.songLengthS += fadeOutS;

	int resoScale = 10;
	int fps = 50 * resoScale;
	song.marSong->ticksPerBeat = 24 * resoScale;
	float bpm = (float)fps * 60 / song.marSong->ticksPerBeat;
	song.marSong->tempoEvents[0].tempo = bpm;
	song.marSong->tempoEvents[0].time = 0;
	song.marSong->numTempoEvents = 1;
	song.tracks.resize(3);
	for (int i = 0; i < 3; i++)
	{
		song.tracks[i].ticks.resize(int(g_args.songLengthS * fps) + 1);
		sprintf_s(song.marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, "Channel %i", i + 1);
	}

	
	// Load tune from file
	std::unique_ptr<SidTune> tune(new SidTune(g_args.inputPath));

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

	// Select default song
	tune->selectSong(g_args.subSong);

	// Configure the engine
	SidConfig cfg;
	cfg.frequency = SAMPLERATE;
	//cfg.samplingMethod = SidConfig::INTERPOLATE;
	cfg.samplingMethod = SidConfig::RESAMPLE_INTERPOLATE;
	cfg.fastSampling = false;
	cfg.playback = SidConfig::MONO;
	cfg.sidEmulation = rs.get();
	cfg.disableAudio = g_args.audioPath[0] == 0;
	//cfg.forceSidModel = true;
	//cfg.forceC64Model = true;
	//cfg.defaultC64Model = SidConfig::c64_model_t::DREAN;
	//cfg.defaultSidModel = SidConfig::sid_model_t::MOS8580;

	if (!m_engine.config(cfg))
		throw m_engine.error();

	// Load tune into engine
	if (!m_engine.load(tune.get()))
		throw m_engine.error();

	wav.clearSamples();
	
	NoteState noteState;

	timeS = 0;
	ticksPerSeconds = bpm / 60 * song.marSong->ticksPerBeat;
	oldTimeT = 0;
	samplesProcessed = 0;
	samplesToPorcess = (int)(SAMPLERATE * g_args.songLengthS);
	samplesBeforeFadeout = samplesToPorcess - (int)(fadeOutS * SAMPLERATE);
}

float SidReader::process()
{
	samplesProcessed += m_engine.play(&buffer.front(), (uint_least32_t)buffer.size());
	timeS = (float)samplesProcessed / SAMPLERATE;
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
			m_engine.getNoteState(noteState, c);

			float freq = noteState.frequency;

			//noteState.isPlaying = true;
			if (noteState.isPlaying && freq >= minFreq && freq <= maxFreq)
			{
				curTick.notePitch = (int)(log2(freq / minFreq) * 12 + 0.5f) + 1;

				if (prevTick.vol == 0 || prevTick.notePitch != curTick.notePitch || noteState.playStateChanged)
					curTick.noteStart = timeT;
				curTick.vol = 50;// (int)ch.amplitude;
				if (noteState.playStateChanged)
					prevTick.vol = 0;
			}
			else
				curTick.vol = 0;
		}
	}

	oldTimeT = timeT;
	if (g_args.audioPath[0] != 0)
	{
		if (samplesProcessed > samplesBeforeFadeout)
		{
			float scale = (float)(samplesToPorcess - samplesProcessed) / (samplesToPorcess - samplesBeforeFadeout);
			for (int i = 0; i < buffer.size(); i++)
				buffer[i] = (int)(buffer[i] * scale);
		}
		wav.addSamples(buffer);
	}

	if (samplesProcessed < samplesToPorcess)
		return (float)samplesProcessed / samplesToPorcess;
	else
	{
		if (g_args.audioPath[0] != 0)
			wav.saveFile(g_args.audioPath);
		song.createNoteList();
		return -1;
	}
}



