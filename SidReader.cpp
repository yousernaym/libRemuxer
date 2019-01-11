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
#include <builders/residfp-builder/residfp.h> 
#include <sidplayfp/sidemu.h>
#include <sidplayfp/SidtuneInfo.h>



//#include "sidspectro\\sidfile.h"
//#include "sidspectro\\c64.h"
#define KERNAL_PATH  "roms\\kernal.901227-02.bin"
#define BASIC_PATH   "roms\\basic.901226-01.bin"
#define CHARGEN_PATH "roms\\characters.901225-01.bin"

#define SAMPLERATE 44100

SidReader::SidReader(Song &song)
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
	
	 // Load ROM files
	char *kernal = loadRom(KERNAL_PATH, 8192);
	char *basic = loadRom(BASIC_PATH, 8192);
	char *chargen = loadRom(CHARGEN_PATH, 4096);

	m_engine.setRoms((const uint8_t*)kernal, (const uint8_t*)basic, (const uint8_t*)chargen);

	if (kernal)
		delete[] kernal;
	if (basic)
		delete[] basic;
	if (chargen)
		delete[] chargen;

	// Set up a SID builder
	std::unique_ptr<ReSIDfpBuilder> rs(new ReSIDfpBuilder("Demo"));

	// Get the number of SIDs supported by the engine
	unsigned int maxsids = (m_engine.info()).maxsids();

	// Create SID emulators
	rs->create(maxsids);

	// Check if builder is ok
	if (!rs->getStatus())
		throw rs->error();

	// Load tune from file
	std::unique_ptr<SidTune> tune(new SidTune(g_args.inputPath));
	
	auto tuneInfo = tune->getInfo();
	
	float PHI;
	int iMinFreq, iMaxFreq;
	if (tuneInfo->clockSpeed() == SidTuneInfo::CLOCK_NTSC)
	{
		PHI = 1022727;
		iMinFreq = 268;
		iMaxFreq = 64832;
	}
	else
	{
		PHI = 985248;
		iMinFreq = 279;
		iMaxFreq = 65535;
	}
	const float freqConSt = 256 * 256 * 256 / PHI;
	const float minFreq = iMinFreq / freqConSt;
	const float maxFreq = iMaxFreq / freqConSt;

	// CHeck if the tune is valid
	if (!tune->getStatus())
		throw tune->statusString();

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

	Wav<short> wav;
	int bufferSize = 500;
	std::vector<short> buffer(bufferSize);
	NoteState noteState;
	
	float timeS = 0;
	float ticksPerSeconds = bpm / 60 * song.marSong->ticksPerBeat;
	int oldTimeT = -1;
	int samplesProcessed = 0;
	int samplesToPorcess = (int)(SAMPLERATE * g_args.songLengthS);
	int samplesBeforeFadeout = samplesToPorcess - (int)(fadeOutS * SAMPLERATE);
	
	for (int i = 0; i < samplesToPorcess; i += samplesProcessed)
	{
		timeS = (float)i / SAMPLERATE;
		int timeT = (int)(timeS * ticksPerSeconds);
		samplesProcessed = m_engine.play(&buffer.front(), bufferSize);

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

				float freq = noteState.frequency / freqConSt;

				//noteState.isPlaying = true;
				if (noteState.isPlaying && freq >= minFreq && freq <= maxFreq)
				{
					curTick.notePitch = (int)(log2(freq  / minFreq) * 12 + 0.5f) + 1;

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
			if (i > samplesBeforeFadeout)
			{
				float scale = (float)(samplesToPorcess - i) / (samplesToPorcess - samplesBeforeFadeout);
				for (int i = 0; i < buffer.size(); i++)
					buffer[i] = (int)(buffer[i] * scale);
			}
			wav.addSamples(buffer);
		}
	}
	if (g_args.audioPath[0] != 0)
		wav.saveFile(g_args.audioPath);
}

SidReader::~SidReader()
{
}



/*
 * Load ROM dump from file.
 * Allocate the buffer if file exists, otherwise return 0.
 */
char* SidReader::loadRom(const char* path, size_t romSize)
{
	char* buffer = 0;
	std::ifstream is(path, std::ios::binary);
	if (is.good())
	{
		buffer = new char[romSize];
		is.read(buffer, romSize);
	}
	is.close();
	return buffer;
}



