#include "SidReader.h"
namespace sid 
{
	int main(Song &song, int argc, char **argv);
};


SidReader::SidReader(Song &song, char *path, BOOL mixdown)
{
	sid::main(song, 1, &path);
}


SidReader::~SidReader()
{
}
