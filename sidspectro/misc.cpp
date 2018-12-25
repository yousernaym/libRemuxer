#include <windows.h>
#include <fstream>
#include <string>
#include <functional>
#include "misc.h"

void LoadBinaryResource(const std::string &path, std::function<void(char*)>OnLoadFunction) 
{
	
	//open file
	std::ifstream infile(path);

	//get length of file
	infile.seekg(0, infile.end);
	size_t length = infile.tellg();
	infile.seekg(0, infile.beg);
	char *arrayBuffer = new char[length];
	//read file
	infile.read(arrayBuffer, length);
	OnLoadFunction(arrayBuffer);
}

//(function() 
//{
//	var requestAnimationFrame = window.requestAnimationFrame || window.mozRequestAnimationFrame ||
//	window.webkitRequestAnimationFrame || window.msRequestAnimationFrame;
//	window.requestAnimationFrame = requestAnimationFrame;
//})();


void DebugMessage(const string &message)
{
	//OutputDebugString(message.c_str());
}

