#pragma once

// Shared helpers for the libopenmpt-backed fixture tests (FxFixtureTests, ModTransportFixtureTests).
// Everything here reads modules through libopenmpt's format-agnostic pattern API so the same
// assertions can run against the XM fixture and its S3M / IT semantic twins.

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "OmptCommands.h"

#include <libopenmpt/libopenmpt.hpp>

namespace ompttest
{

namespace fs = std::filesystem;

// Fixture path relative to test-files/, e.g. "FX.XM" or "mod-transport/loop-E6X.XM".
inline fs::path TestFile(const char *name)
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

inline Cell ReadCell(const openmpt::module &mod, int pattern, int row, int ch)
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

inline bool IsEmpty(const Cell &c)
{
	return c.note == 0 && c.ins == 0 && c.volcmd == 0 && c.vol == 0 && c.fx == 0 && c.param == 0;
}

inline bool IsNote(std::uint8_t note)
{
	return note >= OMPT_NOTE_MIN && note <= OMPT_NOTE_MAX;
}

inline bool IsKeyOff(std::uint8_t note)
{
	return note == OMPT_NOTE_KEYOFF || note == OMPT_NOTE_NOTECUT || note == OMPT_NOTE_FADE;
}

// Exx (MOD/XM) / Sxx (S3M/IT) extended command with the given sub-command nibble.
inline bool IsExOrSx(std::uint8_t fx, std::uint8_t param, int subNibble)
{
	if (fx != CMD_MODCMDEX_ && fx != CMD_S3MCMDEX_)
		return false;
	return ((param >> 4) & 0xf) == subNibble;
}

inline void ExpectNoteIns(const Cell &c, int ins, const char *label)
{
	EXPECT_TRUE(IsNote(c.note)) << label;
	EXPECT_EQ(ins, c.ins) << label;
}

inline void ExpectNoEffect(const Cell &c, const char *label)
{
	EXPECT_EQ(0, c.fx) << label;
	EXPECT_EQ(0, c.param) << label;
	EXPECT_EQ(0, c.volcmd) << label;
	EXPECT_EQ(0, c.vol) << label;
}

inline bool IsSetVol0(const Cell &c)
{
	return (c.fx == CMD_VOLUME_ && c.param == 0) || (c.volcmd == VOLCMD_VOLUME_ && c.vol == 0);
}

enum class ModFormat
{
	Xm,
	S3m,
	It,
};

struct ModFixtureParam
{
	const char *filename;
	ModFormat format;
};

// GoogleTest name for a fixture param: path separators and dots become underscores.
inline std::string FormatName(const testing::TestParamInfo<ModFixtureParam> &info)
{
	std::string name = info.param.filename;
	std::size_t slash = name.find_last_of("/\\");
	if (slash != std::string::npos)
		name = name.substr(slash + 1);
	for (char &ch : name)
	{
		if (ch == '.' || ch == '-')
			ch = '_';
	}
	return name;
}

// Loads GetParam().filename from test-files/ into `mod`. Derive and add TEST_P bodies.
class ModFixtureTest : public testing::TestWithParam<ModFixtureParam>
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

} // namespace ompttest
