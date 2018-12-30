#pragma once
#include <vector>
#include "vic.h"
#include "sp_cpu.h"
#include "cia.h"
#include "sid.h"
#include "sidfile.h"

class C64
{
	friend class MOS6502;
	friend class CIA6526;
	friend class VICII;

private:
	float cyclespersecond = 1000000;
	unsigned char mem[0x10000];
	bool basic_in = true;
	bool kernel_in = true;
	bool char_in = false;
	bool io_in = true;
	int ddr = 0x0; // mem[0]
	int pr = 0x0; // mem[1]
	int count = 0;
	CIA6526 cia1;
	CIA6526 cia2;
	VICII vic;
	MOS6502 cpu;
	//screen = document.getElementById(screenid);
	//canvas = screen.getContext("2d");
	//imagedata = canvas.getImageData(0, 0, screen.width, screen.height);
	//imagedata = canvas.createImageData(800, 400);
public:
	SID6581 sid;
	C64();	
	void Reset();
	int Read(unsigned addr);
	void Write(unsigned addr, int x);
	void MainLoop();
	unsigned GenCopy8(unsigned dest, unsigned src, unsigned memoffset);
	unsigned GenCopy16(unsigned dest, unsigned src, unsigned memoffset);
	int GenStore(int addr, int memoffset);
	int GenLoad(int addr, int memoffset);
	int GenJSR(int addr, int memoffset);
	int GenLDAi(int x, int memoffset);
	void LoadSIDFile(const SIDFile &sidfile);
	float getTime();
};