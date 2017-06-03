#include "SidReader.h"
namespace sid 
{
	int main(Song &song, int argc, const char **argv, double songLengthS);
};


SidReader::SidReader(Song &song, const char *path, double songLengthS)
{
	sid::main(song, 1, &path, songLengthS);
}


SidReader::~SidReader()
{
}
