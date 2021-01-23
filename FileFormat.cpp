#include <assert.h>
#include "FileFormat.h"

FileFormat::FileFormat()
{
}


FileFormat::~FileFormat()
{
}

unsigned FileFormat::swapBytes(size_t size, unsigned value)
{
	assert(size >= 1 && size <= 4);
	if (size == 1)
		return value;
	unsigned bytes[4];
	for (int i = 0; i < 4; i++)
		bytes[i] = value & (0xff << 8 * i);

	if (size == 2)
		return (bytes[0] << 8) | (bytes[1] >> 8);
	else if (size == 3)
		return (bytes[0] << 16) | bytes[1] | (bytes[2] >> 16);
	else if (size == 4)
		return (bytes[0] << 24) | (bytes[1] << 8) | (bytes[2] >> 8) | (bytes[3] >> 24);
	else 
		return 0;
}


void FileFormat::writeLE(size_t size, unsigned value)
{
	outFile.write(reinterpret_cast<const char*>(&value), size);
}

void FileFormat::writeBE(size_t size, unsigned value)
{
	writeLE(size, swapBytes(size, value));
}

void FileFormat::writeByte(unsigned char value)
{
	writeLE(1, value);
}

void FileFormat::writeChunk(void *chunk, size_t size)
{
	outFile.write(reinterpret_cast<const char*>(chunk), size);
}
void FileFormat::createFile(const std::string &path)
{
	outFile.open(path, std::ios::binary | std::ios::out);
}
