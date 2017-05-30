#include "SidReader.h"
namespace sid 
{
	int main(Song &song, int argc, char **argv, double songLengthS);
};


SidReader::SidReader(Song &song, char *path, BOOL mixdown, double songLengthS)
{
	sid::main(song, 1, &path, songLengthS);
}


SidReader::~SidReader()
{
}
