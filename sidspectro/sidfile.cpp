// -------------------------------------------------
// ------------------- sidfile ---------------------
// -------------------------------------------------
#include <sstream>
#include <fstream>
#include "sidfile.h"

SIDFile::SIDFile(const string &path)
{
	loadFile(path);
	if (data.size() < 128) return;
	std::ostringstream oss;
	oss << data[0] << data[1] << data[2] << data[3];
	magicid = oss.str();
	version = data[5] | (data[4]<<8);
	offset = data[7] | (data[6]<<8);
	loadaddr = data[9] | (data[8]<<8);
	initaddr = data[0xB] | (data[0xA]<<8);
	playaddr = data[0xD] | (data[0xC]<<8);
	songs = data[0xF] | (data[0xE]<<8);
	startsong = data[0x11] | (data[0x10]<<8);
	speed = data[0x15] | (data[0x14]<<8) | (data[0x13]<<16) | (data[0x12]<<24);
	startpage = 0x0;
	pagelength = 0x0;

	if (version >= 2)
	{
		startpage = data[0x78];
		pagelength = data[0x79];
	}

	name = "";
	author = "";
	released = "";
	for(int i=0; i<32; i++)
	{
		//name += String.fromCharCode(data[0x16+i]);
		//author += String.fromCharCode(data[0x36+i]);
		//released += String.fromCharCode(data[0x56+i]);
	}

	if (loadaddr == 0)
	{
		loadaddr = data[offset] | (data[offset+1]<<8);
		offset += 2;
	}
	if (initaddr == 0) initaddr = loadaddr;
	endaddr = loadaddr + ((int)data.size() - offset);
}

void SIDFile::loadFile(const string &path)
{
	//open file
	std::ifstream infile(path);

	//get length of file
	infile.seekg(0, infile.end);
	size_t length = infile.tellg();
	infile.seekg(0, infile.beg);
	//read file
	data.resize(length);
	infile.read(&data[0], length);
}
void SIDFile::Print()
{
	//console.log(magicid + " version " + version);
	////console.log("offset " + offset);    
	//console.log("initaddr " + initaddr);
	//console.log("playaddr " + playaddr);
	//console.log("songs " + songs);
	//console.log("startsong " + startsong);
	//console.log("speed " + speed);
	//console.log("startpage " + startpage);
	//console.log("pagelength " + pagelength);
	//console.log("name " + name);
	//console.log("author " + author);
	//console.log("released " + released);
	//if (version >= 2)
	//{
	//	console.log("flags ", data[0x76]);
	//}
	//console.log("loadaddr " + loadaddr);
	//console.log("size " + (data.length-offset));
}

