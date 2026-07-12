#pragma once

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "Song.h"
#include "wav.h"

class SongReader
{
	void clearAudioBuffer()
	{
		std::fill(audioBuffer.begin(), audioBuffer.end(), (SampleType)0);
	}
protected:
	UserArgs userArgs;
	const int sampleRate = 48000;
	Song &song;
	std::vector<SampleType> audioBuffer;
	Wav wav;

	// Per-track WAV pass state (shared by all readers)
	bool mixdownSaved = false;
	float lastReportedProgress = 0;
	std::vector<std::pair<int, std::string>> trackAudioFiles; // midiTrack -> saved path

	void beginProcessing(const UserArgs& args)
	{
		clearAudioBuffer();
		userArgs = args;
		wav.clearSamples();
		mixdownSaved = false;
		lastReportedProgress = 0;
		trackAudioFiles.clear();
	}

	void setAudioBufferSize(size_t size)
	{
		audioBuffer.resize(size);
		clearAudioBuffer();
	}

	// True if the caller requested per-track WAV generation.
	bool trackAudioRequested() const
	{
		return userArgs.trackAudioBasePath != nullptr && userArgs.trackAudioBasePath[0] != 0;
	}

	// Build a per-track WAV path: "<base>-trackNN-<sanitized name>.wav".
	std::string trackAudioPath(int midiTrack, const std::string& trackName) const
	{
		std::string sanitized;
		for (char c : trackName)
		{
			bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '+' || c == '-';
			sanitized += ok ? c : '_';
			if (sanitized.size() >= 40)
				break;
		}
		char buf[16];
		sprintf_s(buf, sizeof(buf), "-track%02d-", midiTrack);
		return std::string(userArgs.trackAudioBasePath) + buf + sanitized + ".wav";
	}

	// Save the current wav buffer as the per-track WAV for midiTrack, then free the buffer.
	void saveTrackWav(int midiTrack, const std::string& name)
	{
		std::string path = trackAudioPath(midiTrack, name);
		if (wav.saveFile(path))
			trackAudioFiles.emplace_back(midiTrack, path);
		wav.clearSamples();
	}

	// Save the full mixdown (main pass) and free the buffer so track passes start clean.
	void saveMixdownNow()
	{
		if (!mixdownSaved && userArgs.audioPath[0] != 0)
			wav.saveFile(userArgs.audioPath);
		mixdownSaved = true;
		wav.clearSamples();
	}

	// Monotonic clamp: progress estimates (pass counts) may shrink between calls.
	float clampProgress(float p)
	{
		if (p < 0)
			return p;
		if (p < lastReportedProgress)
			p = lastReportedProgress;
		lastReportedProgress = p;
		return p;
	}
public:
	SongReader(Song& song, bool stereo, int audioBufferSize = 500) :
		song(song), wav(stereo, sampleRate)
	{
		setAudioBufferSize(audioBufferSize * (stereo ? 2 : 1));
	}
	virtual ~SongReader() {}
	virtual float process() = 0;
	virtual void endProcessing()
	{
		// Save mixdown only if a track pass hasn't already saved it (the cancel-during-main-pass
		// path still writes the partial mixdown; cancel-during-track-pass never overwrites it).
		if (!mixdownSaved && userArgs.audioPath[0] != 0)
			wav.saveFile(userArgs.audioPath);
	}

	const std::vector<std::pair<int, std::string>>& getTrackAudioFiles() const
	{
		return trackAudioFiles;
	}
};
