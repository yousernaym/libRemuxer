#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "../Song.h"
#include "../Wav.h"

namespace fs = std::filesystem;

namespace {

struct SongDataFixture
{
	SongData data{};
	Song song{ &data };

	SongDataFixture()
	{
		data.ticksPerBeat = 48;
		data.numTempoEvents = 0;
		data.numTracks = 0;
		data.tempoEvents = new TempoEvent[16]();
		data.tracks = new SongTrack[MAX_MIDITRACKS]();
		for (int t = 0; t < MAX_MIDITRACKS; t++)
		{
			data.tracks[t].name = new char[MAX_TRACKNAME_LENGTH]();
			data.tracks[t].notes = new SongNote[256]();
			data.tracks[t].numNotes = 0;
		}
	}

	~SongDataFixture()
	{
		for (int t = 0; t < MAX_MIDITRACKS; t++)
		{
			delete[] data.tracks[t].name;
			delete[] data.tracks[t].notes;
		}
		delete[] data.tracks;
		delete[] data.tempoEvents;
	}
};

class TestFileFormat : public FileFormat
{
public:
	using FileFormat::createFile;
	using FileFormat::writeBE;
	using FileFormat::writeLE;
	using FileFormat::outFile;
};

UserArgs MakeArgs(BOOL insTrack, char *midiPath, char *empty)
{
	UserArgs args{};
	args.midiPath = midiPath;
	args.audioPath = empty;
	args.inputPath = empty;
	args.trackAudioBasePath = empty;
	args.insTrack = insTrack;
	return args;
}

bool ContainsBytes(const std::vector<unsigned char> &hay, const unsigned char *needle, size_t n)
{
	if (n == 0 || hay.size() < n)
		return false;
	for (size_t i = 0; i + n <= hay.size(); i++)
	{
		if (std::memcmp(hay.data() + i, needle, n) == 0)
			return true;
	}
	return false;
}

bool ContainsStatus(const std::vector<unsigned char> &hay, unsigned char statusHiNibble)
{
	for (unsigned char b : hay)
	{
		if ((b & 0xF0) == statusHiNibble)
			return true;
	}
	return false;
}

} // namespace

TEST(InstrumentTrack, SampleBasedUsesInstrumentNumber)
{
	EXPECT_EQ(3, Song::instrumentTrack(3, nullptr));
	EXPECT_EQ(-1, Song::instrumentTrack(0, nullptr));
	EXPECT_EQ(-1, Song::instrumentTrack(-1, nullptr));
}

TEST(InstrumentTrack, UsedSetMapsToOneBasedIndex)
{
	std::set<int> used{ 2, 5, 9 };
	EXPECT_EQ(1, Song::instrumentTrack(2, &used));
	EXPECT_EQ(2, Song::instrumentTrack(5, &used));
	EXPECT_EQ(3, Song::instrumentTrack(9, &used));
	EXPECT_EQ(-1, Song::instrumentTrack(7, &used));
}

TEST(CreateNoteList, CollapsesTicksAndScalesTo480Tpb)
{
	SongDataFixture fx;
	fx.song.tracks.resize(1);
	auto& ticks = fx.song.tracks[0].ticks;
	ticks.resize(4);
	// Note sounding from tick 0..2 (ends when vol drops at j=2)
	ticks[0] = RunningTickInfo{ 0, 60, 64, 1 };
	ticks[1] = RunningTickInfo{ 0, 60, 64, 1 };
	ticks[2] = RunningTickInfo{ -1, 0, 0, 0 };
	ticks[3] = RunningTickInfo{ -1, 0, 0, 0 };

	char midiPath[MAX_PATH_LENGTH] = "";
	char empty[1] = "";
	fx.song.createNoteList(MakeArgs(FALSE, midiPath, empty), nullptr);

	// 48 TPB → scale 10 → 480 TPB
	EXPECT_EQ(480, fx.data.ticksPerBeat);
	EXPECT_GE(fx.data.numTracks, 2); // track 0 reserved + channel track
	EXPECT_EQ(1, fx.data.tracks[1].numNotes);
	EXPECT_EQ(60, fx.data.tracks[1].notes[0].pitch);
	EXPECT_EQ(0, fx.data.tracks[1].notes[0].start);
	EXPECT_EQ(20, fx.data.tracks[1].notes[0].stop); // j=2 * scale 10
}

TEST(CreateNoteList, InsTrackMapsInstrumentsAndSkipsUnmapped)
{
	SongDataFixture fx;
	fx.song.tracks.resize(1);
	auto& ticks = fx.song.tracks[0].ticks;
	ticks.resize(8);
	// Instrument 2: ticks 0..2
	ticks[0] = RunningTickInfo{ 0, 60, 64, 2 };
	ticks[1] = RunningTickInfo{ 0, 60, 64, 2 };
	ticks[2] = RunningTickInfo{ -1, 0, 0, 0 };
	// Instrument 5: ticks 3..5
	ticks[3] = RunningTickInfo{ 3, 62, 64, 5 };
	ticks[4] = RunningTickInfo{ 3, 62, 64, 5 };
	ticks[5] = RunningTickInfo{ -1, 0, 0, 0 };
	// Instrument 9 (not in used set): ticks 6..7 — must be skipped
	ticks[6] = RunningTickInfo{ 6, 64, 64, 9 };
	ticks[7] = RunningTickInfo{ -1, 0, 0, 0 };

	std::set<int> used{ 2, 5 };
	char midiPath[MAX_PATH_LENGTH] = "";
	char empty[1] = "";
	fx.song.createNoteList(MakeArgs(TRUE, midiPath, empty), &used);

	EXPECT_EQ(480, fx.data.ticksPerBeat);
	// notes[0] empty + track 1 (ins 2) + track 2 (ins 5)
	EXPECT_EQ(3, fx.data.numTracks);
	EXPECT_EQ(0, fx.data.tracks[0].numNotes);
	ASSERT_EQ(1, fx.data.tracks[1].numNotes);
	EXPECT_EQ(60, fx.data.tracks[1].notes[0].pitch);
	EXPECT_EQ(0, fx.data.tracks[1].notes[0].start);
	EXPECT_EQ(20, fx.data.tracks[1].notes[0].stop);
	ASSERT_EQ(1, fx.data.tracks[2].numNotes);
	EXPECT_EQ(62, fx.data.tracks[2].notes[0].pitch);
	EXPECT_EQ(30, fx.data.tracks[2].notes[0].start); // tick 3 * 10
	EXPECT_EQ(50, fx.data.tracks[2].notes[0].stop);  // tick 5 * 10
}

TEST(CreateNoteList, PitchChangeSplitsNotes)
{
	SongDataFixture fx;
	fx.song.tracks.resize(1);
	auto& ticks = fx.song.tracks[0].ticks;
	ticks.resize(4);
	ticks[0] = RunningTickInfo{ 0, 60, 64, 1 };
	ticks[1] = RunningTickInfo{ 1, 62, 64, 1 }; // new noteStart → closes prior
	ticks[2] = RunningTickInfo{ 1, 62, 64, 1 };
	ticks[3] = RunningTickInfo{ -1, 0, 0, 0 };

	char midiPath[MAX_PATH_LENGTH] = "";
	char empty[1] = "";
	fx.song.createNoteList(MakeArgs(FALSE, midiPath, empty), nullptr);

	ASSERT_EQ(2, fx.data.tracks[1].numNotes);
	EXPECT_EQ(60, fx.data.tracks[1].notes[0].pitch);
	EXPECT_EQ(0, fx.data.tracks[1].notes[0].start);
	EXPECT_EQ(10, fx.data.tracks[1].notes[0].stop);
	EXPECT_EQ(62, fx.data.tracks[1].notes[1].pitch);
	EXPECT_EQ(10, fx.data.tracks[1].notes[1].start);
	EXPECT_EQ(30, fx.data.tracks[1].notes[1].stop);
}

TEST(CreateNoteList, LastTickStillSoundingClosesNote)
{
	SongDataFixture fx;
	fx.song.tracks.resize(1);
	auto& ticks = fx.song.tracks[0].ticks;
	ticks.resize(2);
	ticks[0] = RunningTickInfo{ 0, 60, 64, 1 };
	ticks[1] = RunningTickInfo{ 0, 60, 64, 1 }; // last tick still sounding

	char midiPath[MAX_PATH_LENGTH] = "";
	char empty[1] = "";
	fx.song.createNoteList(MakeArgs(FALSE, midiPath, empty), nullptr);

	ASSERT_EQ(1, fx.data.tracks[1].numNotes);
	EXPECT_EQ(60, fx.data.tracks[1].notes[0].pitch);
	EXPECT_EQ(0, fx.data.tracks[1].notes[0].start);
	EXPECT_EQ(10, fx.data.tracks[1].notes[0].stop); // j=1 * scale 10
}

TEST(CreateNoteList, RescalesTempoEventTimes)
{
	SongDataFixture fx;
	fx.data.numTempoEvents = 1;
	fx.data.tempoEvents[0] = TempoEvent{ 10, 120.0 };
	fx.song.tracks.resize(1);
	fx.song.tracks[0].ticks.resize(1);
	fx.song.tracks[0].ticks[0] = RunningTickInfo{ -1, 0, 0, 0 };

	char midiPath[MAX_PATH_LENGTH] = "";
	char empty[1] = "";
	fx.song.createNoteList(MakeArgs(FALSE, midiPath, empty), nullptr);

	EXPECT_EQ(100, fx.data.tempoEvents[0].time); // 10 * scale 10
}

TEST(SaveMidiFile, WritesFormat1Header)
{
	SongDataFixture fx;
	fx.data.ticksPerBeat = 480;
	fx.data.numTracks = 2;
	fx.data.tracks[1].numNotes = 1;
	fx.data.tracks[1].notes[0] = SongNote{ 0, 480, 60, 0 };
	strcpy_s(fx.data.tracks[1].name, MAX_TRACKNAME_LENGTH, "ch1");

	fs::path path = fs::temp_directory_path() / ("vm_song_" + std::to_string(GetCurrentProcessId()) + ".mid");
	fx.song.saveMidiFile(path.string());
	ASSERT_TRUE(fs::exists(path));

	{
		std::ifstream in(path, std::ios::binary);
		char magic[4]{};
		in.read(magic, 4);
		EXPECT_EQ(0, std::memcmp(magic, "MThd", 4));
		unsigned char hdr[10]{};
		in.read(reinterpret_cast<char*>(hdr), 10);
		// length 6, format 1, 2 tracks, 480 tpb — all big-endian
		EXPECT_EQ(0, hdr[0]); EXPECT_EQ(0, hdr[1]); EXPECT_EQ(0, hdr[2]); EXPECT_EQ(6, hdr[3]);
		EXPECT_EQ(0, hdr[4]); EXPECT_EQ(1, hdr[5]);
		EXPECT_EQ(0, hdr[6]); EXPECT_EQ(2, hdr[7]);
		EXPECT_EQ(0x01, hdr[8]); EXPECT_EQ(0xE0, hdr[9]);
	}
	std::error_code ec;
	fs::remove(path, ec);
}

TEST(SaveMidiFile, WritesTempoMetaAndNoteOnOff)
{
	SongDataFixture fx;
	fx.data.ticksPerBeat = 480;
	fx.data.numTracks = 2;
	fx.data.numTempoEvents = 1;
	fx.data.tempoEvents[0] = TempoEvent{ 0, 120.0 };
	fx.data.tracks[1].numNotes = 1;
	fx.data.tracks[1].notes[0] = SongNote{ 0, 480, 60, 0 };
	strcpy_s(fx.data.tracks[1].name, MAX_TRACKNAME_LENGTH, "ch1");

	fs::path path = fs::temp_directory_path() / ("vm_song_body_" + std::to_string(GetCurrentProcessId()) + ".mid");
	fx.song.saveMidiFile(path.string());
	ASSERT_TRUE(fs::exists(path));

	std::ifstream in(path, std::ios::binary);
	std::vector<unsigned char> bytes(
		(std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
	ASSERT_FALSE(bytes.empty());

	const unsigned char tempoMeta[] = { 0xFF, 0x51 };
	EXPECT_TRUE(ContainsBytes(bytes, tempoMeta, 2));
	EXPECT_TRUE(ContainsStatus(bytes, 0x90)); // note on
	EXPECT_TRUE(ContainsStatus(bytes, 0x80)); // note off

	std::error_code ec;
	fs::remove(path, ec);
}

TEST(FileFormat, WriteLEAndBE)
{
	TestFileFormat ff;
	fs::path path = fs::temp_directory_path() / ("vm_ff_" + std::to_string(GetCurrentProcessId()) + ".bin");
	ASSERT_TRUE(ff.createFile(path.string()));
	ff.writeLE(2, 0x1234);
	ff.writeBE(2, 0x1234);
	ff.outFile.close();

	{
		std::ifstream in(path, std::ios::binary);
		unsigned char b[4]{};
		in.read(reinterpret_cast<char*>(b), 4);
		EXPECT_EQ(0x34, b[0]);
		EXPECT_EQ(0x12, b[1]);
		EXPECT_EQ(0x12, b[2]);
		EXPECT_EQ(0x34, b[3]);
	}
	std::error_code ec;
	fs::remove(path, ec);
}

TEST(Wav, WriteFileHeaderAndNormalizedData)
{
	Wav wav(/*stereo*/ false, 48000);
	std::vector<float> data{ 0.0f, 0.5f, -0.25f, 0.1f };
	fs::path path = fs::temp_directory_path() / ("vm_wav_" + std::to_string(GetCurrentProcessId()) + ".wav");
	ASSERT_TRUE(wav.writeFile(path.string(), data, true));
	ASSERT_TRUE(fs::exists(path));
	ASSERT_GT(fs::file_size(path), 44u);

	{
		std::ifstream in(path, std::ios::binary);
		char riff[4]{}, wave[4]{};
		in.read(riff, 4);
		in.seekg(8);
		in.read(wave, 4);
		EXPECT_EQ(0, std::memcmp(riff, "RIFF", 4));
		EXPECT_EQ(0, std::memcmp(wave, "WAVE", 4));
	}
	std::error_code ec;
	fs::remove(path, ec);
}

TEST(Wav, WriteFile8BitMonoDownmixClampAndPad)
{
	// Stereo source → mono 8-bit PCM (per-channel track-audio path).
	Wav wav(/*stereo*/ true, 48000);
	// Frame 0: downmix (1 + -1) / 2 = 0 → PCM 128
	// Frame 1: clamp 2.0 → 1.0 → PCM 255
	// Frame 2: clamp -2.0 → -1.0 → PCM 0
	// Odd frame count → 1 pad byte after data chunk
	std::vector<float> data{
		1.0f, -1.0f,
		2.0f, 2.0f,
		-2.0f, -2.0f,
	};
	fs::path path = fs::temp_directory_path() / ("vm_wav8_" + std::to_string(GetCurrentProcessId()) + ".wav");
	ASSERT_TRUE(wav.writeFile8BitMono(path.string(), data));
	ASSERT_TRUE(fs::exists(path));

	{
		std::ifstream in(path, std::ios::binary);
		std::vector<unsigned char> bytes(
			(std::istreambuf_iterator<char>(in)),
			std::istreambuf_iterator<char>());
		ASSERT_GE(bytes.size(), 44u + 3u + 1u); // header + 3 samples + pad

		EXPECT_EQ(0, std::memcmp(bytes.data(), "RIFF", 4));
		EXPECT_EQ(0, std::memcmp(bytes.data() + 8, "WAVE", 4));
		EXPECT_EQ(0, std::memcmp(bytes.data() + 12, "fmt ", 4));

		// fmt: PCM (1), mono (1), 8-bit
		EXPECT_EQ(1, bytes[20]); EXPECT_EQ(0, bytes[21]); // format tag
		EXPECT_EQ(1, bytes[22]); EXPECT_EQ(0, bytes[23]); // channels
		EXPECT_EQ(8, bytes[34]); EXPECT_EQ(0, bytes[35]); // bits

		// data chunk at offset 36
		EXPECT_EQ(0, std::memcmp(bytes.data() + 36, "data", 4));
		EXPECT_EQ(3, bytes[40]); // dataSize little-endian (3)
		EXPECT_EQ(0, bytes[41]);
		EXPECT_EQ(0, bytes[42]);
		EXPECT_EQ(0, bytes[43]);
		EXPECT_EQ(128, bytes[44]);
		EXPECT_EQ(255, bytes[45]);
		EXPECT_EQ(1, bytes[46]);   // -1.0 * 127 → -127 → PCM 1
		EXPECT_EQ(0, bytes[47]); // pad
	}
	std::error_code ec;
	fs::remove(path, ec);
}
