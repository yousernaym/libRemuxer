#pragma once
#include <fstream>
#include <string>

class FileFormat
{
private:
	unsigned swapBytes(size_t size, unsigned value);
protected:
	std::ofstream outFile;
	void writeLE(size_t size, unsigned value); //Little endian
	void writeBE(size_t size, unsigned value); //Big endian
	void writeByte(unsigned char value);
	void writeChunk(void *chunk, size_t size);
	void createFile(const std::string &path);
public:
	FileFormat();
	~FileFormat();
};

