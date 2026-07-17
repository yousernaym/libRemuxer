#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "Song.h"
#include "wav.h"

// A saved per-track WAV. channel == -1 for a whole-track WAV (per-channel import mode);
// channel >= 0 for a per-(instrument-track, source-channel) voice WAV (per-instrument mode).
// File scope so the C ABI layer (libRemuxer.cpp) can read its fields.
struct TrackAudioFile
{
	int midiTrack;
	int channel;
	std::string path;
};

class SongReader
{
	void clearAudioBuffer()
	{
		std::fill(audioBuffer.begin(), audioBuffer.end(), (SampleType)0);
	}
protected:
	// A contiguous run of ticks (plus its release tail) owned by one instrument in a channel.
	// endTick == INT_MAX means "to the end of the rendered audio".
	struct InsRun
	{
		int startTick;
		int endTick;
		int ins;
	};

	UserArgs userArgs;
	const int sampleRate = 48000;
	const int channelCount;
	Song &song;
	std::vector<SampleType> audioBuffer;
	std::vector<SampleType> spliceScratch; // reused across voice outputs (see spliceChannelPass)
	Wav wav;

	// Per-track WAV pass state (shared by all readers)
	bool mixdownSaved = false;
	float lastReportedProgress = 0;
	std::vector<TrackAudioFile> trackAudioFiles;

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

	// Sanitize a track name for use inside a WAV filename (letters/digits/space/_+- kept, capped).
	static std::string sanitizeTrackName(const std::string& trackName)
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
		return sanitized;
	}

	// Build a per-track WAV path: "<base>-trackNN-<sanitized name>.wav".
	std::string trackAudioPath(int midiTrack, const std::string& trackName) const
	{
		char buf[16];
		sprintf_s(buf, sizeof(buf), "-track%02d-", midiTrack);
		return std::string(userArgs.trackAudioBasePath) + buf + sanitizeTrackName(trackName) + ".wav";
	}

	// Build a per-voice WAV path: "<base>-trackNN-chCC-<sanitized name>.wav" (deterministic, so a
	// project reload regenerates identical paths and can skip the render passes when they exist).
	std::string trackVoiceAudioPath(int midiTrack, int channel, const std::string& trackName) const
	{
		char buf[24];
		sprintf_s(buf, sizeof(buf), "-track%02d-ch%02d-", midiTrack, channel);
		return std::string(userArgs.trackAudioBasePath) + buf + sanitizeTrackName(trackName) + ".wav";
	}

	// Collapse a channel's per-tick note timeline into instrument-owned runs. Mirrors
	// Song::createNoteList's note-closing conditions exactly, so a run is owned by the same
	// instrument (and thus the same MIDI track) the corresponding note lands on. The tail of each
	// run is then extended to the next run's start, so decay/release between a note and the next
	// note in the channel is attributed to the earlier note's instrument; the final run runs to the
	// end of the rendered audio.
	static std::vector<InsRun> buildInstrumentRuns(const Song::Track& track)
	{
		std::vector<InsRun> runs;
		size_t n = track.ticks.size();
		for (size_t j = 0; j < n; j++)
		{
			const RunningTickInfo& curTick = track.ticks[j];
			const RunningTickInfo& prevTick = track.ticks[j > 0 ? j - 1 : 0];
			if (prevTick.notePitch >= 0 && prevTick.noteStart >= 0 && prevTick.vol > 0 &&
				(curTick.noteStart != prevTick.noteStart || curTick.vol == 0 || j == n - 1))
			{
				runs.push_back({ prevTick.noteStart, (int)j, prevTick.ins });
			}
		}
		for (size_t i = 0; i + 1 < runs.size(); i++)
			runs[i].endTick = runs[i + 1].startTick;
		if (!runs.empty())
			runs.back().endTick = INT_MAX;
		return runs;
	}

	// Slice the just-rendered per-channel pass (accumulated in wav) into one full-length WAV per
	// instrument-track that runs on this channel, gated to the ticks that instrument owns and zero
	// elsewhere. Saves unnormalized (voice levels stay comparable and sum to the channel signal),
	// skips all-silent outputs, records {midiTrack, channel, path}, then clears wav.
	//   tickToSeconds : maps a tick index in track.ticks to seconds (reader-specific timing)
	//   insToMidiTrack: maps a run's instrument id to its MIDI track (< 1 => drop the run)
	void spliceChannelPass(int channel, const Song::Track& track,
		const std::function<double(int)>& tickToSeconds,
		const std::function<int(int)>& insToMidiTrack)
	{
		const std::vector<SampleType>& src = wav.getSamples();
		size_t total = src.size();

		// Group the channel's instrument runs into per-MIDI-track sample ranges (an instrument may
		// recur across the channel, so one track can own several ranges).
		std::map<int, std::vector<std::pair<size_t, size_t>>> trackRanges;
		for (const InsRun& run : buildInstrumentRuns(track))
		{
			int midiTrack = insToMidiTrack(run.ins);
			if (midiTrack < 1)
				continue;
			size_t startSample = (size_t)std::llround(tickToSeconds(run.startTick) * sampleRate) * channelCount;
			size_t endSample = run.endTick >= INT_MAX
				? total
				: (size_t)std::llround(tickToSeconds(run.endTick) * sampleRate) * channelCount;
			if (startSample > total) startSample = total;
			if (endSample > total) endSample = total;
			if (endSample > startSample)
				trackRanges[midiTrack].emplace_back(startSample, endSample);
		}

		for (auto& kv : trackRanges)
		{
			int midiTrack = kv.first;
			spliceScratch.assign(total, (SampleType)0);
			bool anyAudio = false;
			for (auto& range : kv.second)
			{
				for (size_t i = range.first; i < range.second; i++)
				{
					spliceScratch[i] = src[i];
					if (!anyAudio && src[i] != 0)
						anyAudio = true;
				}
			}
			if (!anyAudio)
				continue;
			std::string path = trackVoiceAudioPath(midiTrack, channel, song.songData->tracks[midiTrack].name);
			if (wav.writeFile(path, spliceScratch, false))
				trackAudioFiles.push_back({ midiTrack, channel, path });
		}
		wav.clearSamples();
	}

	// Save the current wav buffer as the whole-track WAV for midiTrack (channel -1), then free it.
	void saveTrackWav(int midiTrack, const std::string& name)
	{
		std::string path = trackAudioPath(midiTrack, name);
		if (wav.saveFile(path))
			trackAudioFiles.push_back({ midiTrack, -1, path });
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
		channelCount(stereo ? 2 : 1), song(song), wav(stereo, sampleRate)
	{
		setAudioBufferSize(audioBufferSize * channelCount);
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

	const std::vector<TrackAudioFile>& getTrackAudioFiles() const
	{
		return trackAudioFiles;
	}
};
