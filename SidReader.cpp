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

//#include "sidspectro\\sidfile.h"
//#include "sidspectro\\c64.h"


SidReader::SidReader(Song &song, const char *path, double songLengthS, int subSong)
{
	initLSPfp(path, subSong);
	process();
	//sid::main(song, 1, &path, songLengthS, subSong);
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
	
	
	//SIDFile sidFile(path);
	//if (subSong > 0)
	//	sidFile.startsong = subSong;

	//C64 c64;
	//c64.LoadSIDFile(sidFile);
	//float timeS = 0;
	//float ticksPerSeconds = bpm / 60 * song.marSong->ticksPerBeat;
	//int oldTimeT = 0;
	//while (timeS < songLengthS)
	//{
	//	c64.MainLoop();
	//	int timeT = (int)(timeS * ticksPerSeconds);
	//	for (int i = 0; i < 3; i++)
	//	{
	//		for (int j = oldTimeT + 1; j < timeT; j++)
	//			song.tracks[i].ticks[j] = song.tracks[i].ticks[oldTimeT];
	//		RunningTickInfo &curTick = song.tracks[i].ticks[timeT];
	//		RunningTickInfo &prevTick = *song.tracks[i].getPrevTick(timeT);

	//		curTick.noteStart = prevTick.noteStart;
	//		SIDChannel ch = c64.sid.channel[i];
	//		if (ch.isPlaying() && ch.frequency >= 20)
	//		{
	//			//float fPitch = log2(ch.frequency / 20) * 12;
	//			//int pitch = (int)fPitch;
	//			//if (abs(fPitch - prevTick.notePitch) <= 0.5f)
	//			//{
	//				//float pitchFrac = fPitch - floor(fPitch);
	//			//}

	//			curTick.notePitch = getPitch(ch.frequency, prevTick.notePitch);
	//			curTick.notePitch = (int)(log2(ch.frequency / 20) * 12 + 0.5f);
	//			
	//			if (prevTick.vol == 0 || prevTick.notePitch != curTick.notePitch)
	//				curTick.noteStart = timeT;
	//			curTick.vol = 50;// (int)ch.amplitude;
	//		}
	//		else
	//			curTick.vol = 0;
	//	}
	//	oldTimeT = timeT;

	//	timeS = c64.getTime();
	//}
}

SidReader::~SidReader()
{
}

#define KERNAL_PATH  ""
#define BASIC_PATH   ""
#define CHARGEN_PATH ""

#define SAMPLERATE 48000

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

int SidReader::initLSPfp(const char *path, int subSong)
{

	{ // Load ROM files
		char *kernal = loadRom(KERNAL_PATH, 8192);
		char *basic = loadRom(BASIC_PATH, 8192);
		char *chargen = loadRom(CHARGEN_PATH, 4096);

		m_engine.setRoms((const uint8_t*)kernal, (const uint8_t*)basic, (const uint8_t*)chargen);

		delete[] kernal;
		delete[] basic;
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
		std::cerr << rs->error() << std::endl;
		return -1;
	}

	// Load tune from file
	std::unique_ptr<SidTune> tune(new SidTune(path));
	//SidTune *tune = new SidTune(path);

	// CHeck if the tune is valid
	if (!tune->getStatus())
	{
		std::cerr << tune->statusString() << std::endl;
		return -1;
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
		std::cerr << m_engine.error() << std::endl;
		return -1;
	}

	// Load tune into engine
	if (!m_engine.load(tune.get()))
	{
		std::cerr << m_engine.error() << std::endl;
		return -1;
	}

	int bufferSize = 10000;
	std::vector<short> buffer(bufferSize);
	for (int i = 0; i < 1000; i++)
	{
		m_engine.play(&buffer.front(), bufferSize / sizeof(short));
		//::write(handle, &buffer.front(), bufferSize);
	}
	return 0;
}

void SidReader::process()
{
	int bufferSize = 10000;
	std::vector<short> buffer(bufferSize);
	for (int i = 0; i < 1000; i++)
	{
		m_engine.play(&buffer.front(), bufferSize / sizeof(short));
		//::write(handle, &buffer.front(), bufferSize);
	}
}
