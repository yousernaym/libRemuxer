#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
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

	UserArgs args{};
	char midiPath[MAX_PATH_LENGTH] = "";
	char empty[1] = "";
	args.midiPath = midiPath;
	args.audioPath = empty;
	args.inputPath = empty;
	args.trackAudioBasePath = empty;
	args.insTrack = FALSE;

	fx.song.createNoteList(args, nullptr);

	// 48 TPB → scale 10 → 480 TPB
	EXPECT_EQ(480, fx.data.ticksPerBeat);
	EXPECT_GE(fx.data.numTracks, 2); // track 0 reserved + channel track
	EXPECT_EQ(1, fx.data.tracks[1].numNotes);
	EXPECT_EQ(60, fx.data.tracks[1].notes[0].pitch);
	EXPECT_EQ(0, fx.data.tracks[1].notes[0].start);
	EXPECT_EQ(20, fx.data.tracks[1].notes[0].stop); // j=2 * scale 10
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
