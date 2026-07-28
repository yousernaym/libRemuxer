// Pattern/order layout for the test-files/mod-transport/ fixtures, which isolate one transport
// (flow-control) effect each: position jump, pattern break, pattern delay, pattern loop, and
// set speed/tempo. Asserted through libopenmpt's format-agnostic pattern API so the hand-authored
// XM and its generated S3M / IT twins share one set of expectations.
// The resulting MIDI (tick positions and note counts) is covered by Remuxer's ModTransportMidiTests.
// Regenerate the twins with `python tests/gen_mod_transport_fixtures.py`.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "OmptCommands.h"
#include "OmptFixtureUtil.h"

#include <libopenmpt/libopenmpt.hpp>

using namespace ompttest;

namespace {

// Shared template: 18 channels, 64 rows per pattern, tempo 125, instrument 1 = a looping sample
// (so a note sustains until it is retriggered or cut, making tick positions readable in the MIDI).
// Only ch0 (effects) and ch1 (note) carry data. XM and S3M store the channel count in the header,
// but IT derives it from the highest channel present in the pattern data (Load_it.cpp), so the IT
// twins collapse to 2 channels — the MIDI track for ch1 is track 2 either way.
void ExpectSharedLayout(openmpt::module &m, ModFormat fmt, int numPatterns)
{
	EXPECT_EQ(fmt == ModFormat::It ? 2 : 18, m.get_num_channels());
	EXPECT_DOUBLE_EQ(125.0, m.get_current_tempo2());
	EXPECT_EQ(numPatterns, m.get_num_patterns());
	for (int p = 0; p < numPatterns; p++)
		EXPECT_EQ(64, m.get_pattern_num_rows(p)) << "pattern " << p;

	ASSERT_GE(m.get_num_samples(), 1);
	EXPECT_TRUE(m.vm_get_sample_loops(1)) << "sample 1 must loop so notes sustain";
}

// Pattern indices actually played, with skip (+++) and stop (---) entries removed.
std::vector<int> PlayableOrders(openmpt::module &m)
{
	std::vector<int> orders;
	for (std::int32_t o = 0; o < m.get_num_orders(); ++o)
	{
		if (m.is_order_stop_entry(o))
			break;
		if (m.is_order_skip_entry(o))
			continue;
		orders.push_back(m.get_order_pattern(o));
	}
	return orders;
}

// The note every fixture plays: OpenMPT note 61 (= XM file note 49 + 12), instrument 1.
void ExpectFixtureNote(const Cell &c, const char *label)
{
	ExpectNoteIns(c, 1, label);
	EXPECT_EQ(61, c.note) << label;
}

// Pattern-loop nibble: E6x in MOD/XM, SBx in S3M/IT — see loopSub in ModReader::readCellFx.
int LoopSub(ModFormat fmt)
{
	return fmt == ModFormat::Xm ? 0x6 : 0xB;
}

class PatternJumpTest : public ModFixtureTest {};
class PatternBreakTest : public ModFixtureTest {};
class RowRepeatTest : public ModFixtureTest {};
class PatternLoopTest : public ModFixtureTest {};
class SpeedTempoTest : public ModFixtureTest {};

} // namespace

// Order 0 row 0 jumps straight to order 1, which carries the note on its row 0.
TEST_P(PatternJumpTest, JumpsToNextOrderCarryingTheNote)
{
	openmpt::module &m = *mod;
	ExpectSharedLayout(m, GetParam().format, 2);
	EXPECT_EQ(6, m.get_current_speed());
	EXPECT_EQ(std::vector<int>({ 0, 1 }), PlayableOrders(m));

	Cell jump = ReadCell(m, 0, 0, 0);
	EXPECT_EQ(CMD_POSITIONJUMP_, jump.fx);
	EXPECT_EQ(1, jump.param);
	EXPECT_FALSE(IsNote(jump.note)) << "the jump row must not play a note";

	Cell note = ReadCell(m, 1, 0, 1);
	ExpectFixtureNote(note, "pattern 1 row 0 ch1");
	EXPECT_EQ(0, note.fx);
	EXPECT_EQ(0, note.param);
}

// Order 0 breaks to row 1 of order 1. That row plays a note and breaks again, but its note channel
// also carries a position jump, so the break's row (1) combines with the jump's order (3) —
// order 2 is skipped entirely.
TEST_P(PatternBreakTest, BreakRowCombinesWithPositionJump)
{
	openmpt::module &m = *mod;
	ExpectSharedLayout(m, GetParam().format, 4);
	EXPECT_EQ(6, m.get_current_speed());
	EXPECT_EQ(std::vector<int>({ 0, 1, 2, 3 }), PlayableOrders(m));

	// Pattern break params are BCD in MOD/XM and S3M but plain hex in IT; OpenMPT normalises
	// both to a row index at load time, so 1 reads back as 1 in all three formats.
	Cell brk = ReadCell(m, 0, 0, 0);
	EXPECT_EQ(CMD_PATTERNBREAK_, brk.fx);
	EXPECT_EQ(1, brk.param);

	Cell brk2 = ReadCell(m, 1, 1, 0);
	EXPECT_EQ(CMD_PATTERNBREAK_, brk2.fx);
	EXPECT_EQ(1, brk2.param);

	Cell noteAndJump = ReadCell(m, 1, 1, 1);
	ExpectFixtureNote(noteAndJump, "pattern 1 row 1 ch1");
	EXPECT_EQ(CMD_POSITIONJUMP_, noteAndJump.fx);
	EXPECT_EQ(3, noteAndJump.param);

	for (int row = 0; row < 64; row++)
		for (int ch = 0; ch < m.get_num_channels(); ch++)
			EXPECT_TRUE(IsEmpty(ReadCell(m, 2, row, ch)))
				<< "pattern 2 must be empty; row " << row << " ch " << ch;

	Cell target = ReadCell(m, 3, 1, 1);
	ExpectFixtureNote(target, "pattern 3 row 1 ch1");
	EXPECT_EQ(0, target.fx);
	EXPECT_EQ(0, target.param);
}

// Pattern delay 2 (EE2 / SE2) plays row 0 three times; the note volume-slides down 4 per tick.
TEST_P(RowRepeatTest, DelayedRowCarriesVolumeSlide)
{
	openmpt::module &m = *mod;
	ExpectSharedLayout(m, GetParam().format, 1);
	EXPECT_EQ(6, m.get_current_speed());
	EXPECT_EQ(std::vector<int>({ 0 }), PlayableOrders(m));

	Cell delay = ReadCell(m, 0, 0, 0);
	EXPECT_TRUE(IsExOrSx(delay.fx, delay.param, 0xE)) << "row 0 ch0 pattern delay";
	EXPECT_EQ(0xE2, delay.param);

	Cell note = ReadCell(m, 0, 0, 1);
	ExpectFixtureNote(note, "row 0 ch1");
	if (GetParam().format == ModFormat::S3m)
	{
		// S3M has no volume-column slides; D04 is the same normal slide down 4 in ModReader.
		EXPECT_EQ(CMD_VOLUMESLIDE_, note.fx);
		EXPECT_EQ(0x04, note.param);
	}
	else
	{
		EXPECT_EQ(VOLCMD_VOLSLIDEDOWN_, note.volcmd);
		EXPECT_EQ(4, note.vol);
	}
}

// Loop start on row 1 (which carries the note) and loop count 2 on row 2, so rows 1-2 play
// three times in total.
TEST_P(PatternLoopTest, LoopSpansTheNoteRowAndTheCountRow)
{
	openmpt::module &m = *mod;
	ExpectSharedLayout(m, GetParam().format, 1);
	EXPECT_EQ(6, m.get_current_speed());
	EXPECT_EQ(std::vector<int>({ 0 }), PlayableOrders(m));

	const int sub = LoopSub(GetParam().format);
	const std::uint8_t base = static_cast<std::uint8_t>(sub << 4);

	EXPECT_TRUE(IsEmpty(ReadCell(m, 0, 0, 0))) << "row 0 ch0 must be empty";

	Cell start = ReadCell(m, 0, 1, 0);
	EXPECT_TRUE(IsExOrSx(start.fx, start.param, sub)) << "row 1 ch0 loop start";
	EXPECT_EQ(base | 0x0, start.param);

	ExpectFixtureNote(ReadCell(m, 0, 1, 1), "row 1 ch1");

	Cell count = ReadCell(m, 0, 2, 0);
	EXPECT_TRUE(IsExOrSx(count.fx, count.param, sub)) << "row 2 ch0 loop count";
	EXPECT_EQ(base | 0x2, count.param);
}

// Row 0 sets speed 3 (F03 / A03), shortening it from the initial 6 ticks to 3; row 1 then drops the
// tempo to 32 (F20 / T20) at the same tick the note starts. The initial speed of 6 is what makes the
// set-speed command load-bearing: without it row 1 would start on module tick 6, not 3.
TEST_P(SpeedTempoTest, SetsSpeedThenTempoOnTheNoteRow)
{
	openmpt::module &m = *mod;
	ExpectSharedLayout(m, GetParam().format, 1);
	EXPECT_EQ(6, m.get_current_speed());
	EXPECT_EQ(std::vector<int>({ 0 }), PlayableOrders(m));

	Cell speed = ReadCell(m, 0, 0, 0);
	EXPECT_EQ(CMD_SPEED_, speed.fx);
	EXPECT_EQ(3, speed.param);

	Cell tempo = ReadCell(m, 0, 1, 0);
	EXPECT_EQ(CMD_TEMPO_, tempo.fx);
	EXPECT_EQ(0x20, tempo.param);

	ExpectFixtureNote(ReadCell(m, 0, 1, 1), "row 1 ch1");
}

INSTANTIATE_TEST_SUITE_P(
	ModTransport,
	PatternJumpTest,
	testing::Values(
		ModFixtureParam{ "mod-transport/pattern-jump-BXX.XM", ModFormat::Xm },
		ModFixtureParam{ "mod-transport/pattern-jump-BXX.S3M", ModFormat::S3m },
		ModFixtureParam{ "mod-transport/pattern-jump-BXX.IT", ModFormat::It }),
	FormatName);

INSTANTIATE_TEST_SUITE_P(
	ModTransport,
	PatternBreakTest,
	testing::Values(
		ModFixtureParam{ "mod-transport/pattern-break-DXX.XM", ModFormat::Xm },
		ModFixtureParam{ "mod-transport/pattern-break-CXX.S3M", ModFormat::S3m },
		ModFixtureParam{ "mod-transport/pattern-break-CXX.IT", ModFormat::It }),
	FormatName);

INSTANTIATE_TEST_SUITE_P(
	ModTransport,
	RowRepeatTest,
	testing::Values(
		ModFixtureParam{ "mod-transport/row-repeat-EEX.XM", ModFormat::Xm },
		ModFixtureParam{ "mod-transport/row-repeat-SEX.S3M", ModFormat::S3m },
		ModFixtureParam{ "mod-transport/row-repeat-SEX.IT", ModFormat::It }),
	FormatName);

INSTANTIATE_TEST_SUITE_P(
	ModTransport,
	PatternLoopTest,
	testing::Values(
		ModFixtureParam{ "mod-transport/loop-E6X.XM", ModFormat::Xm },
		ModFixtureParam{ "mod-transport/loop-SBX.S3M", ModFormat::S3m },
		ModFixtureParam{ "mod-transport/loop-SBX.IT", ModFormat::It }),
	FormatName);

INSTANTIATE_TEST_SUITE_P(
	ModTransport,
	SpeedTempoTest,
	testing::Values(
		ModFixtureParam{ "mod-transport/speed-tempo-FXX.XM", ModFormat::Xm },
		ModFixtureParam{ "mod-transport/speed-tempo-AXX-TXX.S3M", ModFormat::S3m },
		ModFixtureParam{ "mod-transport/speed-tempo-AXX-TXX.IT", ModFormat::It }),
	FormatName);
