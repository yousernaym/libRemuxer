#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../Utf8Path.h"
#include "../libRemuxer.h"

namespace fs = std::filesystem;

TEST(Utf8Path, CopyAcceptsEmptyAndNull)
{
	char buf[16];
	EXPECT_TRUE(copyUtf8Path(buf, sizeof(buf), nullptr));
	EXPECT_STREQ("", buf);
	EXPECT_TRUE(copyUtf8Path(buf, sizeof(buf), ""));
	EXPECT_STREQ("", buf);
}

TEST(Utf8Path, CopyRejectsOversize)
{
	char buf[8];
	std::string longPath(16, 'a');
	EXPECT_FALSE(copyUtf8Path(buf, sizeof(buf), longPath.c_str()));
	EXPECT_STREQ("", buf);

	// Exactly destChars-1 chars + NUL must fit.
	std::string fits(7, 'b');
	EXPECT_TRUE(copyUtf8Path(buf, sizeof(buf), fits.c_str()));
	EXPECT_STREQ("bbbbbbb", buf);
}

TEST(Utf8Path, CopyRejectsZeroDest)
{
	char buf[1] = { 'x' };
	EXPECT_FALSE(copyUtf8Path(buf, 0, "a"));
	EXPECT_EQ('x', buf[0]); // untouched when destChars == 0
}

TEST(Utf8Path, RoundTripOpenWriteNonAscii)
{
	fs::path dir = fs::temp_directory_path() /
		("vm_utf8_café_日本語_" + std::to_string(GetCurrentProcessId()));
	std::error_code ec;
	fs::create_directories(dir, ec);
	ASSERT_FALSE(ec);

	fs::path file = dir / "out.txt";
	// filesystem::path::string() on MSVC is the narrow system encoding; use u8string → UTF-8.
	std::string utf8Path = reinterpret_cast<const char*>(file.u8string().c_str());

	{
		std::ofstream out;
		ASSERT_TRUE(openUtf8Output(out, utf8Path, std::ios::binary | std::ios::trunc));
		out << "hello";
		out.close();
	}

	auto bytes = readUtf8File(utf8Path);
	ASSERT_EQ(5u, bytes.size());
	EXPECT_EQ(0, std::memcmp(bytes.data(), "hello", 5));

	fs::remove_all(dir, ec);
}

TEST(Utf8Path, CopyFitsMaxPathLengthBoundary)
{
	// MAX_PATH_LENGTH includes NUL; max payload is MAX_PATH_LENGTH - 1.
	std::vector<char> dest(MAX_PATH_LENGTH);
	std::string exact(MAX_PATH_LENGTH - 1, 'z');
	EXPECT_TRUE(copyUtf8Path(dest.data(), dest.size(), exact.c_str()));
	EXPECT_EQ(exact, dest.data());

	std::string tooBig(MAX_PATH_LENGTH, 'z');
	EXPECT_FALSE(copyUtf8Path(dest.data(), dest.size(), tooBig.c_str()));
	EXPECT_STREQ("", dest.data());
}
