#include <exception>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>
#include <libopenmpt/libopenmpt.hpp>
#include "wav.h"
#if defined(__clang__)
#if ((__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__) >= 40000)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#endif
#endif

#if defined(__clang__)
#if ((__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__) >= 40000)
#pragma clang diagnostic pop
#endif
#endif

int ompt()
{
	try 
	{
		constexpr std::size_t buffersize = 480;
		constexpr std::int32_t samplerate = 48000;
		std::ifstream file("test.xm", std::ios::binary);
		openmpt::module mod(file);
		
		std::vector<float> left(buffersize);
		std::vector<float> right(buffersize);
		std::vector<float> interleaved_buffer(buffersize * 2);
		bool is_interleaved = true;
		Wav<float> wav(true, 48000);

		while (true) 
		{
			std::size_t count = is_interleaved ? mod.read_interleaved_stereo(samplerate, buffersize, interleaved_buffer.data()) : mod.read(samplerate, buffersize, left.data(), right.data());
			if (count == 0) 
				break;
			wav.addSamples(interleaved_buffer);
		}
		wav.saveFile("mod.wav");
	}
	catch (const std::bad_alloc&) {
		std::cerr << "Error: " << std::string("out of memory") << std::endl;
		return 1;
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << std::string(e.what() ? e.what() : "unknown error") << std::endl;
		return 1;
	}
	return 0;
}