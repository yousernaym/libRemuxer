#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <libopenmpt/libopenmpt.hpp>

namespace fs = std::filesystem;

namespace {

// Mirror ModReader.h OpenMPT command / note constants (stable EffectCommand / VolumeCommand values).
constexpr int OMPT_NOTE_MIN = 1;
constexpr int OMPT_NOTE_MAX = 128;
constexpr int OMPT_NOTE_FADE = 0xFD;
constexpr int OMPT_NOTE_NOTECUT = 0xFE;
constexpr int OMPT_NOTE_KEYOFF = 0xFF;

enum OmptEffect : std::uint8_t
{
	CMD_NONE_ = 0,
	CMD_ARPEGGIO_ = 1,
	CMD_TONEPORTA_ = 4,
	CMD_TONEPORTAVOL_ = 6,
	CMD_VIBRATOVOL_ = 7,
	CMD_VOLUMESLIDE_ = 11,
	CMD_VOLUME_ = 13,
	CMD_RETRIG_ = 15,
	CMD_MODCMDEX_ = 19,
	CMD_S3MCMDEX_ = 20,
};

enum OmptVolCmd : std::uint8_t
{
	VOLCMD_NONE_ = 0,
	VOLCMD_VOLUME_ = 1,
	VOLCMD_VOLSLIDEUP_ = 3,
	VOLCMD_VOLSLIDEDOWN_ = 4,
};

fs::path TestFile(const char *name)
{
	fs::path p = fs::path(__FILE__).parent_path() / ".." / "test-files" / name;
	return fs::weakly_canonical(p);
}

struct Cell
{
	std::uint8_t note = 0;
	std::uint8_t ins = 0;
	std::uint8_t volcmd = 0;
	std::uint8_t vol = 0;
	std::uint8_t fx = 0;
	std::uint8_t param = 0;
};

Cell ReadCell(const openmpt::module &mod, int pattern, int row, int ch)
{
	using C = openmpt::module::command_index;
	Cell c;
	c.note = mod.get_pattern_row_channel_command(pattern, row, ch, C::command_note);
	c.ins = mod.get_pattern_row_channel_command(pattern, row, ch, C::command_instrument);
	c.volcmd = mod.get_pattern_row_channel_command(pattern, row, ch, C::command_volumeffect);
	c.vol = mod.get_pattern_row_channel_command(pattern, row, ch, C::command_volume);
	c.fx = mod.get_pattern_row_channel_command(pattern, row, ch, C::command_effect);
	c.param = mod.get_pattern_row_channel_command(pattern, row, ch, C::command_parameter);
	return c;
}

bool IsNote(std::uint8_t note)
{
	return note >= OMPT_NOTE_MIN && note <= OMPT_NOTE_MAX;
}

bool IsKeyOff(std::uint8_t note)
{
	return note == OMPT_NOTE_KEYOFF || note == OMPT_NOTE_NOTECUT || note == OMPT_NOTE_FADE;
}

bool IsExOrSx(std::uint8_t fx, std::uint8_t param, int subNibble)
{
	if (fx != CMD_MODCMDEX_ && fx != CMD_S3MCMDEX_)
		return false;
	return ((param >> 4) & 0xf) == subNibble;
}

void ExpectNoteIns(const Cell &c, int ins, const char *label)
{
	EXPECT_TRUE(IsNote(c.note)) << label;
	EXPECT_EQ(ins, c.ins) << label;
}

void ExpectNoEffect(const Cell &c, const char *label)
{
	EXPECT_EQ(0, c.fx) << label;
	EXPECT_EQ(0, c.param) << label;
	EXPECT_EQ(0, c.volcmd) << label;
	EXPECT_EQ(0, c.vol) << label;
}

bool IsSetVol0(const Cell &c)
{
	return (c.fx == CMD_VOLUME_ && c.param == 0) || (c.volcmd == VOLCMD_VOLUME_ && c.vol == 0);
}

enum class FxFormat
{
	Xm,
	S3m,
	It,
};

struct FxFixtureParam
{
	const char *filename;
	FxFormat format;
};

std::string FormatName(const testing::TestParamInfo<FxFixtureParam> &info)
{
	std::string name = info.param.filename;
	for (char &ch : name)
	{
		if (ch == '.')
			ch = '_';
	}
	return name;
}

class FxFixtureTest : public testing::TestWithParam<FxFixtureParam>
{
protected:
	std::vector<std::uint8_t> data;
	std::unique_ptr<openmpt::module> mod;

	void SetUp() override
	{
		fs::path path = TestFile(GetParam().filename);
		ASSERT_TRUE(fs::exists(path)) << path.string();
		std::ifstream in(path, std::ios::binary);
		ASSERT_TRUE(in) << path.string();
		data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		ASSERT_FALSE(data.empty());
		std::ostringstream log;
		mod = std::make_unique<openmpt::module>(data, log);
	}
};

} // namespace

TEST_P(FxFixtureTest, MatchesDescribedPatternAndSpeed)
{
	const FxFormat fmt = GetParam().format;
	openmpt::module &m = *mod;

	EXPECT_EQ(6, m.get_current_speed());
	EXPECT_EQ(125.0, m.get_current_tempo2());
	EXPECT_EQ(18, m.get_num_channels());
	EXPECT_GE(m.get_num_patterns(), 1);
	EXPECT_EQ(64, m.get_pattern_num_rows(0));

	ASSERT_GE(m.get_num_samples(), 2);
	EXPECT_TRUE(m.vm_get_sample_loops(1)) << "sample 1 should loop";
	EXPECT_FALSE(m.vm_get_sample_loops(2)) << "sample 2 should not loop";
	EXPECT_EQ(10, m.vm_get_sample_length(2));

	auto C = [&](int row, int ch) { return ReadCell(m, 0, row, ch); };

	// Ch0: note + EC1 / SC1
	{
		Cell c = C(0, 0);
		ExpectNoteIns(c, 1, "ch0");
		EXPECT_TRUE(IsExOrSx(c.fx, c.param, 0xC));
		EXPECT_EQ(0xC1, c.param);
	}
	// Ch1: note with instrument 2; OpenMPT note 62 (XM file note 50 + 12; S3M/IT twins match)
	{
		Cell c = C(0, 1);
		ExpectNoteIns(c, 2, "ch1");
		EXPECT_EQ(0, c.fx);
		EXPECT_EQ(0, c.param);
		EXPECT_EQ(62, c.note);
	}
	// Ch2: note; set vol 0 next row
	{
		ExpectNoteIns(C(0, 2), 1, "ch2");
		EXPECT_TRUE(IsSetVol0(C(1, 2))) << "ch2 row1 set vol 0";
	}
	// Ch3: note + ED1 / SD1; note-off next row
	{
		Cell c = C(0, 3);
		ExpectNoteIns(c, 1, "ch3");
		EXPECT_TRUE(IsExOrSx(c.fx, c.param, 0xD));
		EXPECT_EQ(0xD1, c.param);
		EXPECT_TRUE(IsKeyOff(C(1, 3).note));
	}
	// Ch4: note; note-off next row
	{
		ExpectNoteIns(C(0, 4), 1, "ch4");
		EXPECT_TRUE(IsKeyOff(C(1, 4).note));
	}
	// Ch5: note; note-off row 1; non-zero instrument row 2
	{
		ExpectNoteIns(C(0, 5), 1, "ch5");
		EXPECT_TRUE(IsKeyOff(C(1, 5).note));
		EXPECT_NE(0, C(2, 5).ins);
	}
	// Ch6: volume slide down 2 for 7 rows
	{
		Cell c0 = C(0, 6);
		ExpectNoteIns(c0, 1, "ch6");
		EXPECT_EQ(CMD_VOLUMESLIDE_, c0.fx);
		EXPECT_EQ(0x02, c0.param);
		for (int r = 1; r <= 6; r++)
		{
			Cell c = C(r, 6);
			EXPECT_EQ(CMD_VOLUMESLIDE_, c.fx) << "ch6 row " << r;
			EXPECT_EQ(0x00, c.param) << "ch6 row " << r;
		}
		ExpectNoEffect(C(7, 6), "ch6 row 7");
	}
	// Ch7: vol-column slide down 2 (XM/IT) or effect-column volume slide (S3M)
	{
		ExpectNoteIns(C(0, 7), 1, "ch7");
		if (fmt == FxFormat::S3m)
		{
			EXPECT_EQ(CMD_VOLUMESLIDE_, C(0, 7).fx);
			EXPECT_EQ(0x02, C(0, 7).param);
			for (int r = 1; r <= 6; r++)
			{
				EXPECT_EQ(CMD_VOLUMESLIDE_, C(r, 7).fx) << "ch7 row " << r;
				EXPECT_EQ(0x00, C(r, 7).param) << "ch7 row " << r;
			}
		}
		else
		{
			EXPECT_EQ(VOLCMD_VOLSLIDEDOWN_, C(0, 7).volcmd);
			EXPECT_EQ(2, C(0, 7).vol);
			for (int r = 1; r <= 6; r++)
			{
				EXPECT_EQ(VOLCMD_VOLSLIDEDOWN_, C(r, 7).volcmd) << "ch7 row " << r;
				EXPECT_EQ(2, C(r, 7).vol) << "ch7 row " << r;
			}
		}
		ExpectNoEffect(C(7, 7), "ch7 row 7");
	}
	// Ch8: note; toneporta+vol slide then continue × 6
	{
		ExpectNoteIns(C(0, 8), 1, "ch8");
		EXPECT_EQ(CMD_TONEPORTAVOL_, C(1, 8).fx);
		EXPECT_EQ(0x02, C(1, 8).param);
		for (int r = 2; r <= 7; r++)
		{
			EXPECT_EQ(CMD_TONEPORTAVOL_, C(r, 8).fx) << "ch8 row " << r;
			EXPECT_EQ(0x00, C(r, 8).param) << "ch8 row " << r;
		}
		ExpectNoEffect(C(8, 8), "ch8 row 8");
	}
	// Ch9: note; vibrato+vol slide then continue × 6
	{
		ExpectNoteIns(C(0, 9), 1, "ch9");
		EXPECT_EQ(CMD_VIBRATOVOL_, C(1, 9).fx);
		EXPECT_EQ(0x02, C(1, 9).param);
		for (int r = 2; r <= 7; r++)
		{
			EXPECT_EQ(CMD_VIBRATOVOL_, C(r, 9).fx) << "ch9 row " << r;
			EXPECT_EQ(0x00, C(r, 9).param) << "ch9 row " << r;
		}
		ExpectNoEffect(C(8, 9), "ch9 row 8");
	}
	// Ch10: fine volume down — EB2 (XM) or DF2 (S3M/IT) for 32 rows
	{
		ExpectNoteIns(C(0, 10), 1, "ch10");
		if (fmt == FxFormat::Xm)
		{
			EXPECT_TRUE(IsExOrSx(C(0, 10).fx, C(0, 10).param, 0xB));
			EXPECT_EQ(0xB2, C(0, 10).param);
			for (int r = 1; r <= 31; r++)
			{
				EXPECT_TRUE(IsExOrSx(C(r, 10).fx, C(r, 10).param, 0xB)) << "ch10 row " << r;
				EXPECT_EQ(0xB0, C(r, 10).param) << "ch10 row " << r;
			}
		}
		else
		{
			// DF2 then D00 (memory). DF0 is normal slide-up F in ModReader, not fine-down mem.
			EXPECT_EQ(CMD_VOLUMESLIDE_, C(0, 10).fx);
			EXPECT_EQ(0xF2, C(0, 10).param);
			for (int r = 1; r <= 31; r++)
			{
				EXPECT_EQ(CMD_VOLUMESLIDE_, C(r, 10).fx) << "ch10 row " << r;
				EXPECT_EQ(0x00, C(r, 10).param) << "ch10 row " << r;
			}
		}
		ExpectNoEffect(C(32, 10), "ch10 row 32");
	}
	// Ch11: arpeggio 025; note-off next row
	{
		Cell c = C(0, 11);
		ExpectNoteIns(c, 1, "ch11");
		EXPECT_EQ(CMD_ARPEGGIO_, c.fx);
		EXPECT_EQ(0x25, c.param);
		EXPECT_TRUE(IsKeyOff(C(1, 11).note));
	}
	// Ch12: note on row 0 and row 1
	{
		ExpectNoteIns(C(0, 12), 1, "ch12 r0");
		ExpectNoteIns(C(1, 12), 1, "ch12 r1");
	}
	// Ch13: E92 (XM) or Q02 (S3M/IT) retrigger; note-off next row
	{
		Cell c = C(0, 13);
		ExpectNoteIns(c, 1, "ch13");
		if (fmt == FxFormat::Xm)
		{
			EXPECT_TRUE(IsExOrSx(c.fx, c.param, 0x9));
			EXPECT_EQ(0x92, c.param);
		}
		else
		{
			EXPECT_EQ(CMD_RETRIG_, c.fx);
			EXPECT_EQ(0x02, c.param);
		}
		EXPECT_TRUE(IsKeyOff(C(1, 13).note));
	}
	// Ch14: porta — note rows 0–1; row 1 tone porta; pitch +1
	{
		Cell a = C(0, 14);
		Cell b = C(1, 14);
		ExpectNoteIns(a, 1, "ch14 r0");
		ExpectNoteIns(b, 1, "ch14 r1");
		EXPECT_EQ(CMD_TONEPORTA_, b.fx);
		EXPECT_EQ(0x00, b.param);
		EXPECT_EQ(a.note + 1, b.note);
	}
	// Ch15: same with toneporta+vol
	{
		Cell a = C(0, 15);
		Cell b = C(1, 15);
		ExpectNoteIns(a, 1, "ch15 r0");
		ExpectNoteIns(b, 1, "ch15 r1");
		EXPECT_EQ(CMD_TONEPORTAVOL_, b.fx);
		EXPECT_EQ(0x00, b.param);
		EXPECT_EQ(a.note + 1, b.note);
	}
	// Ch16: vol slide up F, down F, up 1 (XM vol column; S3M/IT effect Dxy)
	{
		ExpectNoteIns(C(0, 16), 1, "ch16");
		if (fmt == FxFormat::Xm)
		{
			EXPECT_EQ(VOLCMD_VOLSLIDEUP_, C(0, 16).volcmd);
			EXPECT_EQ(0xF, C(0, 16).vol);
			EXPECT_EQ(VOLCMD_VOLSLIDEDOWN_, C(1, 16).volcmd);
			EXPECT_EQ(0xF, C(1, 16).vol);
			EXPECT_EQ(VOLCMD_VOLSLIDEUP_, C(2, 16).volcmd);
			EXPECT_EQ(1, C(2, 16).vol);
		}
		else
		{
			// DF0 = normal up F; D0F = normal down F; D10 = up 1
			EXPECT_EQ(CMD_VOLUMESLIDE_, C(0, 16).fx);
			EXPECT_EQ(0xF0, C(0, 16).param);
			EXPECT_EQ(CMD_VOLUMESLIDE_, C(1, 16).fx);
			EXPECT_EQ(0x0F, C(1, 16).param);
			EXPECT_EQ(CMD_VOLUMESLIDE_, C(2, 16).fx);
			EXPECT_EQ(0x10, C(2, 16).param);
		}
	}
	// Ch17: note at zero volume then volume +1
	{
		Cell c0 = C(0, 17);
		ExpectNoteIns(c0, 1, "ch17");
		EXPECT_TRUE(IsSetVol0(c0)) << "ch17 row0 set vol 0";
		if (fmt == FxFormat::S3m)
		{
			EXPECT_EQ(CMD_VOLUMESLIDE_, C(1, 17).fx);
			EXPECT_EQ(0x10, C(1, 17).param);
		}
		else
		{
			EXPECT_EQ(VOLCMD_VOLSLIDEUP_, C(1, 17).volcmd);
			EXPECT_EQ(1, C(1, 17).vol);
		}
	}
}

INSTANTIATE_TEST_SUITE_P(
	FxModules,
	FxFixtureTest,
	testing::Values(
		FxFixtureParam{ "FX.XM", FxFormat::Xm },
		FxFixtureParam{ "FX.S3M", FxFormat::S3m },
		FxFixtureParam{ "FX.IT", FxFormat::It }),
	FormatName);
