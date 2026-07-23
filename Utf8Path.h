#pragma once
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>

// Paths arrive as UTF-8 from the managed Remuxer front-end (LPUTF8Str). MSVC narrow
// iostreams / CRT fopen use the ANSI code page, so convert before opening.

// Copy a UTF-8 path into a fixed buffer. Returns false if it does not fit (including NUL);
// on failure dest is cleared when destChars > 0. Prefer this over strcpy_s — oversize
// paths must not invoke the CRT invalid-parameter handler.
inline bool copyUtf8Path(char *dest, size_t destChars, const char *utf8)
{
	if (!dest || destChars == 0)
		return false;
	if (!utf8)
	{
		dest[0] = '\0';
		return true;
	}
	size_t len = strlen(utf8);
	if (len + 1 > destChars)
	{
		dest[0] = '\0';
		return false;
	}
	memcpy(dest, utf8, len + 1);
	return true;
}

inline std::wstring utf8ToWide(const std::string &utf8)
{
	if (utf8.empty())
		return {};
	int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
	if (needed <= 0)
		return {};
	std::wstring wide(static_cast<size_t>(needed - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], needed);
	return wide;
}

inline bool openUtf8Input(std::ifstream &stream, const std::string &utf8Path)
{
	stream.open(utf8ToWide(utf8Path), std::ios::binary);
	return stream.is_open();
}

inline bool openUtf8Output(std::ofstream &stream, const std::string &utf8Path, std::ios::openmode mode)
{
	stream.open(utf8ToWide(utf8Path), mode);
	return stream.is_open();
}

inline std::vector<unsigned char> readUtf8File(const std::string &utf8Path)
{
	std::ifstream in;
	if (!openUtf8Input(in, utf8Path))
		return {};
	in.seekg(0, std::ios::end);
	std::streamoff size = in.tellg();
	if (size <= 0)
		return {};
	in.seekg(0, std::ios::beg);
	std::vector<unsigned char> buf(static_cast<size_t>(size));
	in.read(reinterpret_cast<char*>(buf.data()), size);
	if (!in)
		return {};
	return buf;
}
