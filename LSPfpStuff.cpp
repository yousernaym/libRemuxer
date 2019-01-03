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
#include <builders/residfp-builder/residfp.h>

 /**
  * Compile with
  *     g++ `pkg-config --cflags libsidplayfp` `pkg-config --libs libsidplayfp` demo.cpp
  */

  /*
   * Adjust these paths to point to existing ROM dumps if needed.
   */
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
	std::auto_ptr<ReSIDfpBuilder> rs(new ReSIDfpBuilder("Demo"));

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
	std::auto_ptr<SidTune> tune(new SidTune(path));

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

	
	// Play
	int bufferSize = 100;
	std::vector<short> buffer(bufferSize);
	for (int i = 0; i < 1000; i++)
	{
		m_engine.play(&buffer.front(), bufferSize / sizeof(short));
		//::write(handle, &buffer.front(), bufferSize);
	}

}
