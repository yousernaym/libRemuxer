#pragma once
#include <string>
#include <vector>

using string = std::string;

class SIDFile
{
public:
	std::vector<unsigned char> data;
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
	~SIDFile();

	void loadFile(const string &path);
	void Print();
};