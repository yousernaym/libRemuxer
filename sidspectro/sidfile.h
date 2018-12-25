#pragma once
#include <string>
#include <vector>

using string = std::string;

class SIDFile
{
public:
	//if (dataLength < 128) return;
	std::vector<char> data;
	string magicid;
	int version;
	int offset;
	int loadaddr;
	int initaddr;
	int playaddr;
	int endaddr;
	int songs;
	int startsong;
	int speed;
	int startpage;
	int pagelength;
	string name;
	string author;
	string released;
//public:
	SIDFile(const string &path);
	void loadFile(const string &path);
	void Print();
};