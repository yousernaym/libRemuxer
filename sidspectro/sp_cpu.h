#pragma once
#include "functional"
// -------------------------------------------------
// ---------------------- CPU ----------------------
// -------------------------------------------------

class MOS6502
{
public:
	unsigned char *mem;
	std::function<void(int, int)> Write;
	std::function<int(int)> Read;
	int AC = 0x0;
	int XR = 0x0;
	int YR = 0x0;
	int SP = 0x0;
	int PC = 0x0;
	bool Carry_Flag = false;
	bool Zero_Flag = false;
	bool IRQ_Disable_Flag = false;
	bool Decimal_Flag = false;
	bool Brk_Flag = false;
	bool Unused_Flag = false;
	bool Overflow_Flag = false;
	int Negative_Flag = 0;
	unsigned char ops[256];
//public:
	MOS6502(std::function<int(int)>_read, std::function<void(int, int)>_write, unsigned char *_mem);
	void Reset();
	void SetPC(int offset);
	int GetStatus();
	void PutStatus(int x);
	void Disassemble();
	void MaskableInterrupt();
	void NonMaskableInterrupt();
	int Step(int steps);
};