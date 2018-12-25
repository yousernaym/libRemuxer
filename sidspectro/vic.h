#pragma once
#include <functional>
// -------------------------------------------------
// -------------------- VIC II ---------------------
// -------------------------------------------------

class VICII
{
public:
	std::function<void()> interruptfunc;;
	unsigned char *mem;
	unsigned char regs[64];
	unsigned char colorram[1024];
	int cycles = 0;
	int rasterline = 0x0;
	int bank = 0;
	bool ecm = false;
	bool mcm = false;
	bool bmm = false;
	int r = 0x0;
	int h0 = 0x0;
	int h1 = 0x0;
	int h2 = 0x0;
	int h3 = 0x0;
	int scrollx = 0x0;
	int scrolly = 0x0;
	int zeichenram = 0x0;
	int charram = 0x0;
	bool charen = false;
//public:
	VICII(unsigned char *_mem, std::function<void()> _interruptfunc);
	void VICII::Reset();
	int Read(int addr);
	void SetBank(int _bank);
	void Write(int addr, int x);
	void CalcSteps(int count);
};