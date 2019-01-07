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


//#include "sidspectro\\sidfile.h"
//#include "sidspectro\\c64.h"
#define KERNAL_PATH  "d:\\hämta mh\\kodning\\MUSIC\\c64\\kernal.901227-03.bin"
#define BASIC_PATH   "d:\\hämta mh\\kodning\\MUSIC\\c64\\basic.901226-01.bin"
#define CHARGEN_PATH ""

#define SAMPLERATE 44100

SidReader::SidReader(Song &song, const char *path, double songLengthS, int subSong)
{
	//songLengthS = 5;
	if (songLengthS == 0)
		songLengthS = 300;
	//process(path, subSong, songLengthS);
	
	int fps = 60;
	song.marSong->ticksPerBeat = 24;
	float bpm = (float)fps * 60 / song.marSong->ticksPerBeat;
	song.marSong->tempoEvents[0].tempo = bpm;
	song.marSong->tempoEvents[0].time = 0;
	song.marSong->numTempoEvents = 1;
	song.tracks.resize(3);
	for (int i = 0; i < 3; i++)
	{
		song.tracks[i].ticks.resize(int(songLengthS * fps) + 1);
		sprintf_s(song.marSong->tracks[i + 1].name, MAX_TRACKNAME_LENGTH, "Channel %i", i + 1);
	}
	
	{ // Load ROM files
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
	}

	// Set up a SID builder
	std::unique_ptr<ReSIDfpBuilder> rs(new ReSIDfpBuilder("Demo"));
	//ReSIDfpBuilder *rs = new ReSIDfpBuilder("Demo");


	// Get the number of SIDs supported by the engine
	unsigned int maxsids = (m_engine.info()).maxsids();

	// Create SID emulators
	rs->create(maxsids);

	// Check if builder is ok
	if (!rs->getStatus())
	{
		throw rs->error();
	}

	// Load tune from file
	std::unique_ptr<SidTune> tune(new SidTune(path));
	//SidTune *tune = new SidTune(path);

	// CHeck if the tune is valid
	if (!tune->getStatus())
	{
		throw tune->statusString();
	}

	// Select default song
	tune->selectSong(subSong);

	// Configure the engine
	SidConfig cfg;
	cfg.frequency = SAMPLERATE;
	cfg.samplingMethod = SidConfig::INTERPOLATE;
	cfg.fastSampling = false;
	cfg.playback = SidConfig::MONO;
	cfg.sidEmulation = rs.get();
	if (!m_engine.config(cfg))
	{
		throw m_engine.error();
	}

	// Load tune into engine
	if (!m_engine.load(tune.get()))
	{
		throw m_engine.error();
	}

	Wav<short> wav;
	int bufferSize = 1000;
	std::vector<short> buffer(bufferSize);
	SIDChannel channelState;
	
	float timeS = 0;
	float ticksPerSeconds = bpm / 60 * song.marSong->ticksPerBeat;
	int oldTimeT = 0;
	for (int i = 0; i < (int)((double)SAMPLERATE / bufferSize * songLengthS) + 1; i++)
	{
		m_engine.play(&buffer.front(), bufferSize);
		//m_engine.config().sidEmulation.
		
		int timeT = (int)(timeS * ticksPerSeconds);
		for (int c = 0; c < 3; c++)
		{
			for (int t = oldTimeT + 1; t < timeT; t++)
				song.tracks[c].ticks[t] = song.tracks[c].ticks[oldTimeT];
			RunningTickInfo &curTick = song.tracks[c].ticks[timeT];
			RunningTickInfo &prevTick = *song.tracks[c].getPrevTick(timeT);

			curTick.noteStart = prevTick.noteStart;
			m_engine.getSIDChannel(channelState, c);

			if (channelState.isPlaying && channelState.frequency >= 20)
			{
				curTick.notePitch = (int)(log2(channelState.frequency / 20) * 12 + 0.5f);

				if (prevTick.vol == 0 || prevTick.notePitch != curTick.notePitch)
					curTick.noteStart = timeT;
				curTick.vol = 50;// (int)ch.amplitude;
			}
			else
				curTick.vol = 0;
		}
		oldTimeT = timeT;
		timeS = (float)i * bufferSize / SAMPLERATE;

		wav.addSamples(buffer);
	}
	wav.createFile("output.wav");
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



